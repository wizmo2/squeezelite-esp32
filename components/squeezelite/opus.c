/*
 *  Squeezelite - lightweight headless squeezebox emulator
 *
 *  (c) Adrian Smith 2012-2015, triode1@btinternet.com
 *      Ralph Irving 2015-2017, ralph_irving@hotmail.com
 *		Philippe 2018-2019, philippe_44@outlook.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "squeezelite.h"

/* 
*  with some low-end CPU, the decode call takes a fair bit of time and if the outputbuf is locked during that
*  period, the output_thread (or equivalent) will be locked although there is plenty of samples available.
*  Normally, with PRIO_INHERIT, that thread should increase decoder priority and get the lock quickly but it
*  seems that when the streambuf has plenty of data, the decode thread grabs the CPU to much, even it the output
*  thread has a higher priority. Using an interim buffer where opus decoder writes the output is not great from
*  an efficiency (one extra memory copy) point of view, but it allows the lock to not be kept for too long
*/

#if BYTES_PER_FRAME == 4		
#define ALIGN(n) 	(n)
#else
#define ALIGN(n) 	(n << 16)		
#endif

#include <ogg/ogg.h>
#include <opus.h>

// opus maximum output frames is 120ms @ 48kHz
#define MAX_OPUS_FRAMES 5760

struct opus {
	enum { OGG_ID_HEADER, OGG_COMMENT_HEADER } status;
	ogg_stream_state state;
	ogg_packet packet;
	ogg_sync_state sync;
	ogg_page page;
	OpusDecoder* decoder;
	int rate, gain, pre_skip;
	size_t overframes;
	u8_t *overbuf;
	int channels;
};

#if !LINKALL
static struct {
	void *handle;
	int (*ogg_stream_init)(ogg_stream_state* os, int serialno);
	int (*ogg_stream_clear)(ogg_stream_state* os);
	int (*ogg_stream_reset)(ogg_stream_state* os);
	int (*ogg_stream_eos)(ogg_stream_state* os);
	int (*ogg_stream_reset_serialno)(ogg_stream_state* os, int serialno);
	int (*ogg_sync_clear)(ogg_sync_state* oy);
	void (*ogg_packet_clear)(ogg_packet* op);
	char* (*ogg_sync_buffer)(ogg_sync_state* oy, long size);
	int (*ogg_sync_wrote)(ogg_sync_state* oy, long bytes);
	long (*ogg_sync_pageseek)(ogg_sync_state* oy, ogg_page* og);
	int (*ogg_sync_pageout)(ogg_sync_state* oy, ogg_page* og);
	int (*ogg_stream_pagein)(ogg_stream_state* os, ogg_page* og);
	int (*ogg_stream_packetout)(ogg_stream_state* os, ogg_packet* op);
	int (*ogg_page_packets)(const ogg_page* og);
} go;

static struct {
	void* handle;
	OpusDecoder* (*opus_decoder_create)(opus_int32 Fs, int channels, int* error);
	int (*opus_decode)(OpusDecoder* st, const unsigned char* data, opus_int32 len, opus_int16* pcm, int frame_size, int decode_fec);
	void (*opus_decoder_destroy)(OpusDecoder* st);
} gu;
#endif

static struct opus *u;

extern log_level loglevel;

extern struct buffer *streambuf;
extern struct buffer *outputbuf;
extern struct streamstate stream;
extern struct outputstate output;
extern struct decodestate decode;
extern struct processstate process;

#define LOCK_S   mutex_lock(streambuf->mutex)
#define UNLOCK_S mutex_unlock(streambuf->mutex)
#define LOCK_O   mutex_lock(outputbuf->mutex)
#define UNLOCK_O mutex_unlock(outputbuf->mutex)
#if PROCESS
#define LOCK_O_direct   if (decode.direct) mutex_lock(outputbuf->mutex)
#define UNLOCK_O_direct if (decode.direct) mutex_unlock(outputbuf->mutex)
#define IF_DIRECT(x)    if (decode.direct) { x }
#define IF_PROCESS(x)   if (!decode.direct) { x }
#else
#define LOCK_O_direct   mutex_lock(outputbuf->mutex)
#define UNLOCK_O_direct mutex_unlock(outputbuf->mutex)
#define IF_DIRECT(x)    { x }
#define IF_PROCESS(x)
#endif

#if LINKALL
#define OG(h, fn, ...) (ogg_ ## fn)(__VA_ARGS__)
#define OP(h, fn, ...) (opus_ ## fn)(__VA_ARGS__)
#else
#define OG(h, fn, ...) (h)->ogg_ ## fn(__VA_ARGS__)
#define OP(h, fn, ...) (h)->opus_ ## fn(__VA_ARGS__)
#endif

static unsigned parse_uint16(const unsigned char* _data) {
	return _data[0] | _data[1] << 8;
}

static int parse_int16(const unsigned char* _data) {
	return ((_data[0] | _data[1] << 8) ^ 0x8000) - 0x8000;
}

static opus_uint32 parse_uint32(const unsigned char* _data) {
	return _data[0] | (opus_uint32)_data[1] << 8 |
		(opus_uint32)_data[2] << 16 | (opus_uint32)_data[3] << 24;
}

static int get_audio_packet(void) {
	int status, packet = -1;

	LOCK_S;
	size_t bytes = min(_buf_used(streambuf), _buf_cont_read(streambuf));

	while (!(status = OG(&go, stream_packetout, &u->state, &u->packet)) && bytes) {

		// if sync_pageout (or sync_pageseek) is not called here, sync buffers build up
		while (!(status = OG(&go, sync_pageout, &u->sync, &u->page)) && bytes) {
			size_t consumed = min(bytes, 4096);
			char* buffer = OG(&go, sync_buffer, &u->sync, consumed);
			memcpy(buffer, streambuf->readp, consumed);
			OG(&go, sync_wrote, &u->sync, consumed);

			_buf_inc_readp(streambuf, consumed);
			bytes -= consumed;
		}

		// if we have a new page, put it in and reset serialno at BoS
		if (status) {
			OG(&go, stream_pagein, &u->state, &u->page);
			if (OG(&go, page_bos, &u->page)) OG(&go, stream_reset_serialno, &u->state, OG(&go, page_serialno, &u->page));
		}
	}

	/* discard header packets. With no packet, we return a negative value 
	 * when there is really nothing more to proceed */
	if (status > 0 && memcmp(u->packet.packet, "OpusHead", 8) && memcmp(u->packet.packet, "OpusTags", 8)) packet = status;
	else if (stream.state > DISCONNECT || _buf_used(streambuf)) packet = 0;

	UNLOCK_S;
	return packet;
}

static int read_opus_header(void) {
	int done = 0;
	bool fetch = true;

	LOCK_S;
	size_t bytes = min(_buf_used(streambuf), _buf_cont_read(streambuf));

	while (bytes && !done) {
		int status;

		// get aligned to a page and ready to bring it in
		do {
			size_t consumed = min(bytes, 4096);

			char* buffer = OG(&go, sync_buffer, &u->sync, consumed);
			memcpy(buffer, streambuf->readp, consumed);
			OG(&go, sync_wrote, &u->sync, consumed);

			_buf_inc_readp(streambuf, consumed);
			bytes -= consumed;

			status = fetch ? OG(&go, sync_pageout, &u->sync, &u->page) :
							 OG(&go, sync_pageseek, &u->sync, &u->page);
		} while (bytes && status <= 0);

		// nothing has been found and we have no more bytes, come back later
		if (status <= 0) break;

		// always set stream serialno if we have a new one
		if (OG(&go, page_bos, &u->page)) OG(&go, stream_reset_serialno, &u->state, OG(&go, page_serialno, &u->page));

		// bring new page in if we want it (otherwise we're just skipping)
		if (fetch) OG(&go, stream_pagein, &u->state, &u->page);

		// no need for a switch...case
		if (u->status == OGG_ID_HEADER) {
			// we need the id packet, get more pages if we don't
			if (OG(&go, stream_packetout, &u->state, &u->packet) <= 0) continue;
			
			// make sure this is a valid packet
			if (u->packet.bytes < 19 || memcmp(u->packet.packet, "OpusHead", 8)) {
				LOG_ERROR("wrong header packet (size:%u)", u->packet.bytes);
				done = -100;
			} else {
				u->status = OGG_COMMENT_HEADER;
				u->channels = u->packet.packet[9];
				u->pre_skip = parse_uint16(u->packet.packet + 10);
				u->rate = parse_uint32(u->packet.packet + 12);
				u->gain = parse_int16(u->packet.packet + 16);
				u->decoder = OP(&gu, decoder_create, 48000, u->channels, &status);
				fetch = false;
				if (!u->decoder || status != OPUS_OK) {
					LOG_ERROR("can't create decoder %d (channels:%u)", status, u->channels);
				}
				else {
					LOG_INFO("codec up and running");
				}
			}
		} else if (u->status == OGG_COMMENT_HEADER) {
			// don't consume VorbisComment which could be a huge packet, just skip it
			if (!OG(&go, page_packets, &u->page)) continue;
            LOG_INFO("comment skipped successfully");
			done = 1;
		}
	}

	UNLOCK_S;
	return done;
}

static decode_state opus_decompress(void) {
	frames_t frames;
	u8_t *write_buf;

	if (decode.new_stream) {      
        int status = read_opus_header();

		if (status == 0) {
            return DECODE_RUNNING;
		} else if (status < 0) {
			LOG_WARN("can't create codec");
			return DECODE_ERROR;
		}
        
		LOCK_O;
		output.next_sample_rate = decode_newstream(48000, output.supported_rates);
		IF_DSD(	output.next_fmt = PCM; )
		output.track_start = outputbuf->writep;
		if (output.fade_mode) _checkfade(true);
		decode.new_stream = false;
		UNLOCK_O;
        
        if (u->channels > 2) {
			LOG_WARN("too many channels: %d", u->channels);
			return DECODE_ERROR;
		}
        
		LOG_INFO("setting track_start");
	}

	LOCK_O_direct;
	IF_DIRECT(
		frames = min(_buf_space(outputbuf), _buf_cont_write(outputbuf)) / BYTES_PER_FRAME;
		write_buf = outputbuf->writep;
	);
	IF_PROCESS(
		frames = process.max_in_frames;
		write_buf = process.inbuf;
	);
    
    int packet, n = 0;
	
    // get some packets and decode them, or use the leftover from previous pass
    if (u->overframes) {
		/* use potential leftover from previous encoding. We know that it will fit this time
		 * as min_space is >=MAX_OPUS_FRAMES and we start from the beginning of the buffer */
		memcpy(write_buf, u->overbuf, u->overframes * BYTES_PER_FRAME);
		n = u->overframes;
		u->overframes = 0;
	} else if ((packet = get_audio_packet()) > 0) {
		if (frames < MAX_OPUS_FRAMES) {
			// don't have enough contiguous space, use the overflow buffer
			n = OP(&gu, decode, u->decoder, u->packet.packet, u->packet.bytes, (opus_int16*) u->overbuf, MAX_OPUS_FRAMES, 0);
			if (n > 0) {
				u->overframes = n - min(n, frames);
				n = min(n, frames);
				memcpy(write_buf, u->overbuf, n * BYTES_PER_FRAME);
				memmove(u->overbuf, u->overbuf + n, u->overframes);
			}
		} else {
			/* we just do one packet at a time, although we could loop on packets but that means locking the 
			 * outputbuf and streambuf for maybe a long time while we process it all, so don't do that */
			n = OP(&gu, decode, u->decoder, u->packet.packet, u->packet.bytes, (opus_int16*) write_buf, frames, 0);
		}
	} else if (!packet) {
		UNLOCK_O_direct;
		return DECODE_RUNNING;
	}
			
	if (n > 0) {
		frames_t count;
		s16_t *iptr;
		ISAMPLE_T *optr;

		frames = n;
		count = frames * u->channels;

		// work backward to unpack samples (if needed)
		iptr = (s16_t *) write_buf + count;
		IF_DIRECT(
			optr = (ISAMPLE_T *) outputbuf->writep + frames * 2;
		)
		IF_PROCESS(
			optr = (ISAMPLE_T *) write_buf + frames * 2;
		)
		
		if (u->channels == 2) {
#if BYTES_PER_FRAME == 8
			while (count--) {
				*--optr = ALIGN(*--iptr);
			}
#endif
		} else if (u->channels == 1) {
			while (count--) {
				*--optr = ALIGN(*--iptr);
				*--optr = ALIGN(*iptr);
			}
		}

		IF_DIRECT(
			_buf_inc_writep(outputbuf, frames * BYTES_PER_FRAME);
		);
		IF_PROCESS(
			process.in_frames = frames;
		);

		LOG_SDEBUG("wrote %u frames", frames);

	} else if (n == 0) {

		if (packet < 0) {
			LOG_INFO("end of decode");
			UNLOCK_O_direct;
			return DECODE_COMPLETE;
		} else {
			LOG_INFO("no frame decoded");
        }

	} else {

		LOG_INFO("decode error: %d", n);
		UNLOCK_O_direct;
		return DECODE_COMPLETE;
	}

	UNLOCK_O_direct;
	return DECODE_RUNNING;
}

static void opus_open(u8_t size, u8_t rate, u8_t chan, u8_t endianness) {   
    if (u->decoder) OP(&gu, decoder_destroy, u->decoder);         
    u->decoder = NULL;
    
	if (!u->overbuf) u->overbuf = malloc(MAX_OPUS_FRAMES * BYTES_PER_FRAME);
    
    u->status = OGG_ID_HEADER;
	u->overframes = 0;

    OG(&go, stream_clear, &u->state);	
    OG(&go, sync_clear, &u->sync);
    OG(&go, stream_init, &u->state, -1);    
}

static void opus_close(void) {  
	if (u->decoder) OP(&gu, decoder_destroy, u->decoder);
    u->decoder = NULL;
    
	free(u->overbuf);
    u->overbuf = NULL;
    
    OG(&go, stream_clear, &u->state);
    OG(&go, sync_clear, &u->sync);
}

static bool load_opus(void) {
#if !LINKALL
	char *err;
    
	void *u.handle = dlopen(LIBOPUS, RTLD_NOW);  
	if (!u_handle) {
		LOG_INFO("opus dlerror: %s", dlerror());
		return false;
	}

    void *g_handle = dlopen(LIBOGG, RTLD_NOW);    
    if (!g_handle) {
        dlclose(u_handle);
		LOG_INFO("ogg dlerror: %s", dlerror());
		return false;
	}
	
	g_handle->ogg_stream_clear = dlsym(g_handle->handle, "ogg_stream_clear");
	g_handle->ogg_stream_reset = dlsym(g_handle->handle, "ogg_stream_reset");
	g_handle->ogg_stream_eos = dlsym(g_handle->handle, "ogg_stream_eos");
	g_handle->ogg_stream_reset_serialno = dlsym(g_handle->handle, "ogg_stream_reset_serialno");
	g_handle->ogg_sync_clear = dlsym(g_handle->handle, "ogg_sync_clear");
	g_handle->ogg_packet_clear = dlsym(g_handle->handle, "ogg_packet_clear");
	g_handle->ogg_sync_buffer = dlsym(g_handle->handle, "ogg_sync_buffer");
	g_handle->ogg_sync_wrote = dlsym(g_handle->handle, "ogg_sync_wrote");
	g_handle->ogg_sync_pageseek = dlsym(g_handle->handle, "ogg_sync_pageseek");
	g_handle->ogg_sync_pageout = dlsym(g_handle->handle, "ogg_sync_pageout");
	g_handle->ogg_stream_pagein = dlsym(g_handle->handle, "ogg_stream_pagein");
	g_handle->ogg_stream_packetout = dlsym(g_handle->handle, "ogg_stream_packetout");
	g_handle->ogg_page_packets = dlsym(g_handle->handle, "ogg_page_packets");
    
	u_handle->opus_decoder_create = dlsym(u_handle->handle, "opus_decoder_create");
	u_handle->opus_decoder_destroy = dlsym(u_handle->handle, "opus_decoder_destroy");
	u_handle->opus_decode = dlsym(u_handle->handle, "opus_decode");
	
	if ((err = dlerror()) != NULL) {
		LOG_INFO("dlerror: %s", err);
		return false;
	}

	LOG_INFO("loaded "LIBOPUS);
#endif

	return true;
}

struct codec *register_opus(void) {
	static struct codec ret = {
		'u',          // id
		"ops",        // types
		8*1024,       // min read
		MAX_OPUS_FRAMES*BYTES_PER_FRAME*2,       // min space
		opus_open, 	  // open
		opus_close,   // close
		opus_decompress,  // decode
	};

	if ((u = calloc(1, sizeof(struct opus))) == NULL) {
		return NULL;
	}

	if (!load_opus()) {
		return NULL;
	}

	LOG_INFO("using opus to decode ops");
	return &ret;
}

