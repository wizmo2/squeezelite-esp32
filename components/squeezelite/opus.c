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
	enum {OGG_SYNC, OGG_ID_HEADER, OGG_COMMENT_HEADER} status;
	ogg_stream_state state;
	ogg_packet packet;
	ogg_sync_state sync;
	ogg_page page;
	OpusDecoder* decoder;
	int rate, gain, pre_skip;
	bool fetch;
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
#define LOCK_O_not_direct   if (!decode.direct) mutex_lock(outputbuf->mutex)
#define UNLOCK_O_not_direct if (!decode.direct) mutex_unlock(outputbuf->mutex)
#define IF_DIRECT(x)    if (decode.direct) { x }
#define IF_PROCESS(x)   if (!decode.direct) { x }
#else
#define LOCK_O_direct   mutex_lock(outputbuf->mutex)
#define UNLOCK_O_direct mutex_unlock(outputbuf->mutex)
#define LOCK_O_not_direct
#define UNLOCK_O_not_direct
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

static int get_opus_packet(void) {
	int status = 0;

	LOCK_S;
	size_t bytes = min(_buf_used(streambuf), _buf_cont_read(streambuf));
	
	while (!(status = OG(&go, stream_packetout, &u->state, &u->packet)) && bytes) {
		do {
			size_t consumed = min(bytes, 4096);
			char* buffer = OG(&gu, sync_buffer, &u->sync, consumed);
			memcpy(buffer, streambuf->readp, consumed);
			OG(&gu, sync_wrote, &u->sync, consumed);

			_buf_inc_readp(streambuf, consumed);
			bytes -= consumed;
		} while (!(status = OG(&gu, sync_pageseek, &u->sync, &u->page)) && bytes);

		// if we have a new page, put it in
		if (status)	OG(&go, stream_pagein, &u->state, &u->page);
	} 

	UNLOCK_S;
	return status;
}

static int read_opus_header(void) {
	int status = 0;

	LOCK_S;
	size_t bytes = min(_buf_used(streambuf), _buf_cont_read(streambuf));

	while (bytes && !status) {

		// first fetch a page if we need one
		if (u->fetch) {
			size_t consumed = min(bytes, 4096);
			char* buffer = OG(&gu, sync_buffer, &u->sync, consumed);
			memcpy(buffer, streambuf->readp, consumed);
			OG(&gu, sync_wrote, &u->sync, consumed);

			_buf_inc_readp(streambuf, consumed);
			bytes -= consumed;

			if (!OG(&gu, sync_pageseek, &u->sync, &u->page)) continue;
			u->fetch = false;
		}

		//bytes = min(bytes, size);
		switch (u->status) {
		case OGG_SYNC:
			u->status = OGG_ID_HEADER;
			OG(&gu, stream_reset_serialno, &u->state, OG(&gu, page_serialno, &u->page));
			break;
		case OGG_ID_HEADER:
			status = OG(&gu, stream_pagein, &u->state, &u->page);
			if (OG(&gu, stream_packetout, &u->state, &u->packet)) {
				if (u->packet.bytes < 19 || memcmp(u->packet.packet, "OpusHead", 8)) {
					LOG_ERROR("wrong opus header packet (size:%u)", u->packet.bytes);
					status = -100;
					break;
				}
				u->status = OGG_COMMENT_HEADER;                
				u->channels = u->packet.packet[9];
				u->pre_skip = parse_uint16(u->packet.packet + 10);
				u->rate = parse_uint32(u->packet.packet + 12);
				u->gain = parse_int16(u->packet.packet + 16);
				u->decoder = OP(&gu, decoder_create, 48000, u->channels, &status);
				if (!u->decoder || status != OPUS_OK) {
					LOG_ERROR("can't create decoder %d (channels:%u)", status, u->channels);
				}
			}
			u->fetch = true;
			break;
		case OGG_COMMENT_HEADER:
			// loop until we have consumed VorbisComment and get ready for a new packet
			u->fetch = true;
			status = OG(&gu, page_packets, &u->page);
			break;
		default:
			break;
		}
	}

	UNLOCK_S;
	return status;
}

static decode_state opus_decompress(void) {
	frames_t frames;
	int n;
	static int channels;
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
        
        channels = u->channels;

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
	
    // get some packets and decode them, or use the leftover from previous pass
    if (u->overframes) {
		/* use potential leftover from previous encoding. We know that it will fit this time
		 * as min_space is >=MAX_OPUS_FRAMES and we start from the beginning of the buffer */
		memcpy(write_buf, u->overbuf, u->overframes * BYTES_PER_FRAME);
		n = u->overframes;
		u->overframes = 0;
	} else if (get_opus_packet() > 0) {
		if (frames < MAX_OPUS_FRAMES) {
			// don't have enough contiguous space, use the overflow buffer (still works if n < 0)
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
	} else if (!OG(&go, page_eos, &u->page)) {
		UNLOCK_O_direct;
		return DECODE_RUNNING;
	}
			
	if (n > 0) {
		frames_t count;
		s16_t *iptr;
		ISAMPLE_T *optr;

		frames = n;
		count = frames * channels;

		// work backward to unpack samples (if needed)
		iptr = (s16_t *) write_buf + count;
		IF_DIRECT(
			optr = (ISAMPLE_T *) outputbuf->writep + frames * 2;
		)
		IF_PROCESS(
			optr = (ISAMPLE_T *) write_buf + frames * 2;
		)
		
		if (channels == 2) {
#if BYTES_PER_FRAME == 8
			while (count--) {
				*--optr = ALIGN(*--iptr);
			}
#endif
		} else if (channels == 1) {
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

		if (stream.state <= DISCONNECT) {
			LOG_INFO("end of decode");
			UNLOCK_O_direct;
			return DECODE_COMPLETE;
		} else {
			LOG_INFO("no frame decoded");
        }

	} else {

		LOG_INFO("opus decode error: %d", n);
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
    
    u->status = OGG_SYNC;
	u->fetch = true;
	u->overframes = 0;
	
	OG(&gu, sync_init, &u->sync);
	OG(&gu, stream_init, &u->state, -1);
}

static void opus_close(void) {  
	if (u->decoder) OP(&gu, decoder_destroy, u->decoder);
    u->decoder = NULL;
    
	free(u->overbuf);
    u->overbuf = NULL;
    
	OG(&gu, stream_clear, &u->state);
	OG(&gu, sync_clear, &u->sync);
}

static bool load_opus(void) {
#if !LINKALL
	char *err;
    void *g_handle = dlopen(LIBOGG, RTLD_NOW);
	void *u.handle = dlopen(LIBOPUS, RTLD_NOW);
    
	if (!g_handle || !u_handle) {
		LOG_INFO("dlerror: %s", dlerror());
		return false;
	}
	
	g_handle->ogg_stream_clear = dlsym(g_handle->handle, "ogg_stream_clear");
	g_handle->.ogg_stream_reset = dlsym(g_handle->handle, "ogg_stream_reset");
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

