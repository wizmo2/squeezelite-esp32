/* 
 *  Squeezelite - lightweight headless squeezebox emulator
 *
 *  (c) Adrian Smith 2012-2015, triode1@btinternet.com
 *      Ralph Irving 2015-2017, ralph_irving@hotmail.com
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

// automatically select between floating point (preferred) and fixed point libraries:
// NOTE: works with Tremor version here: http://svn.xiph.org/trunk/Tremor, not vorbisidec.1.0.2 currently in ubuntu

#include <ogg/ogg.h>
#ifdef TREMOR_ONLY
#include <vorbis/ivorbiscodec.h>
#else
#include <vorbis/codec.h>
static bool tremor = false;
#endif

// this is tremor packing, not mine...
static inline int32_t clip15(int32_t x) {
  int ret = x;
  ret -= ((x<=32767)-1)&(x-32767);
  ret -= ((x>=-32768)-1)&(x+32768);
  return ret;
}

#if BYTES_PER_FRAME == 4		
#define ALIGN(n) clip15((n) >> 9);
#define ALIGN_FLOAT(n) ((n)*32768.0f + 0.5f)
#else
#define ALIGN(n) (clip15((n) >> 9) << 16)
#define ALIGN_FLOAT ((n)*32768.0f*65536.0f + 0.5f)
#endif

struct vorbis {
	bool opened;
	enum { OGG_ID_HEADER, OGG_COMMENT_HEADER, OGG_SETUP_HEADER } status;
	struct {
		ogg_stream_state state;
		ogg_packet packet;
		ogg_sync_state sync;
		ogg_page page;
	};
	struct {
		vorbis_dsp_state decoder;
		vorbis_info info;
		vorbis_comment comment;
		vorbis_block block;
	};
	int rate, channels;
    uint32_t overflow;
};

#if !LINKALL
static struct vorbis {
	// vorbis symbols to be dynamically loaded - from either vorbisfile or vorbisidec (tremor) version of library
	vorbis_info *(* ov_info)(OggVorbis_File *vf, int link);
	int (* ov_clear)(OggVorbis_File *vf);
	void (* ov_info_init)(vorbis_info* vi);
	void (* ov_info_clear)(vorbis_info* vi);
    void (* ov_comment_init)(vorbis_comment* vc);
    void (* ov_comment_clear(vorbis_comment *vc);
	int (* ov_block_init)(vorbis_dsp_state* v, vorbis_block* vb);
	int (* ov_block_clear)(vorbis_block* vb);
	void (* ov_vorbis_dsp_clear)(vorbis_dsp_state* v);    
	long (* ov_read)(OggVorbis_File *vf, char *buffer, int length, int bigendianp, int word, int sgned, int *bitstream);
	long (* ov_read_tremor)(OggVorbis_File *vf, char *buffer, int length, int *bitstream);
	int (* ov_open_callbacks)(void *datasource, OggVorbis_File *vf, const char *initial, long ibytes, ov_callbacks callbacks);
} gv;

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
#endif

static struct vorbis *v;

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
#define OV(h, fn, ...) (vorbis_ ## fn)(__VA_ARGS__)
#define OG(h, fn, ...) (ogg_ ## fn)(__VA_ARGS__)
#else
#define OV(h, fn, ...) (h)->ov_##fn(__VA_ARGS__)
#define OG(h, fn, ...) (h)->ogg_ ## fn(__VA_ARGS__)
#endif

static int get_audio_packet(void) {
	int status, packet = -1;

	LOCK_S;
	size_t bytes = min(_buf_used(streambuf), _buf_cont_read(streambuf));

	while (!(status = OG(&go, stream_packetout, &v->state, &v->packet)) && bytes) {
		
		// if sync_pageout (or sync_pageseek) is not called here, sync buffers build up
		while (!(status = OG(&go, sync_pageout, &v->sync, &v->page)) && bytes) {
			size_t consumed = min(bytes, 4096);
			char* buffer = OG(&go, sync_buffer, &v->sync, consumed);
			memcpy(buffer, streambuf->readp, consumed);
			OG(&go, sync_wrote, &v->sync, consumed);

			_buf_inc_readp(streambuf, consumed);
			bytes -= consumed;
		}

		// if we have a new page, put it in and reset serialno at BoS
		if (status) {
			OG(&go, stream_pagein, &v->state, &v->page);
			if (OG(&go, page_bos, &v->page)) OG(&go, stream_reset_serialno, &v->state, OG(&go, page_serialno, &v->page));	
		}
	}

	/* odd packets are not audio and should be discarded. With no packet, we
	 * return a negative value when there is really nothing more to proceed */
	if (status > 0 && (v->packet.packet[0] & 0x01) == 0) packet = status;
	else if (stream.state > DISCONNECT || _buf_used(streambuf)) packet = 0;

	UNLOCK_S;
	return packet;
}

static int read_vorbis_header(void) {
	int done = 0;
	bool fetch = true;

	LOCK_S;
	size_t bytes = min(_buf_used(streambuf), _buf_cont_read(streambuf));

	while (bytes && !done) {
		int status;

		// get aligned to a page and ready to bring it in
		do {
			size_t consumed = min(bytes, 4096);

			char* buffer = OG(&go, sync_buffer, &v->sync, consumed);
			memcpy(buffer, streambuf->readp, consumed);
			OG(&go, sync_wrote, &v->sync, consumed);

			_buf_inc_readp(streambuf, consumed);
			bytes -= consumed;

			status = fetch ? OG(&go, sync_pageout, &v->sync, &v->page) :
		 					 OG(&go, sync_pageseek, &v->sync, &v->page);
		} while (bytes && status <= 0);

		// nothing has been found and we have no more bytes, come back later
		if (status <= 0) break;

		// always set stream serialno if we have a new one
		if (OG(&go, page_bos, &v->page)) OG(&go, stream_reset_serialno, &v->state, OG(&go, page_serialno, &v->page));

		// bring new page in if we want it (otherwise we're just skipping)
		if (fetch) OG(&go, stream_pagein, &v->state, &v->page);

		// not a switch...case b/c we might have multiple packets in a page in vorbis
		if (v->status == OGG_ID_HEADER) {
			// we need the id packet, get more pages if we don't
			if (!OG(&go, stream_packetout, &v->state, &v->packet)) continue;

			OV(&gv, info_init, &v->info);
			status = OV(&gv, synthesis_headerin, &v->info, &v->comment, &v->packet);

			if (status) {
				LOG_ERROR("id header packet error %d", status);
				done = -1;
			} else {
				v->channels = v->info.channels;
				v->rate = v->info.rate;
				v->status = OGG_COMMENT_HEADER;
				fetch = false;
				LOG_INFO("id acquired");
				// we should only have one packet, so get next pages
				if (OG(&go, page_packets, &v->page) == 1) continue;
			}
		} 
		
		if (v->status == OGG_COMMENT_HEADER) {
			// don't consume VorbisComment which could be a huge packet, just skip it
			int packets = OG(&go, page_packets, &v->page);
			if (!packets) continue;

			// we have a "fake" comment packet that is just has the last page...
			v->status = OGG_SETUP_HEADER;
			OG(&go, stream_pagein, &v->state, &v->page);
			OG(&go, stream_packetout, &v->state, &v->packet);

			OV(&gv, comment_init, &v->comment);
			v->comment.vendor = "N/A";
			fetch = true;
			LOG_INFO("comment skipped successfully");

			// because of lack of page alignment, we might have the setup page already fully in
			if (packets == 1) continue;
		}

		if (v->status == OGG_SETUP_HEADER) {
			// we need the setup packet, get more pages if we don't
			if (OG(&go, stream_packetout, &v->state, &v->packet) <= 0) continue;

			// finally build a codec if we have the packet
			if (OV(&gv, synthesis_headerin, &v->info, &v->comment, &v->packet) ||
				OV(&gv, synthesis_init, &v->decoder, &v->info)) {
				LOG_ERROR("setup header packet error %d", status);
				// no need to free comment, it's fake
				OV(&gv, info_clear, &v->info);
				done = -1;
			} else {
				OV(&gv, block_init, &v->decoder, &v->block);
				v->opened = true;
				LOG_INFO("codec up and running");
				done = 1;
			}
			break;
		}
	}

	UNLOCK_S;
	return done;
}

inline int pcm_out(vorbis_dsp_state* decoder, void*** pcm) {
#ifndef TREMOR_ONLY                
    if (!tremor) return OV(&gv, synthesis_pcmout, decoder, (ogg_float_t***) pcm);
#endif                
    return OV(&gv, synthesis_pcmout, decoder, (ogg_int32_t***) pcm);
}      

static decode_state vorbis_decode(void) {
	frames_t frames;
	u8_t *write_buf;
    
	if (decode.new_stream) {
        int status = read_vorbis_header();

		if (status == 0) {
			return DECODE_RUNNING;
		} else if (status < 0) {
			LOG_WARN("can't create codec");
			return DECODE_ERROR;
		}

		LOCK_O;
		output.next_sample_rate = decode_newstream(v->rate, output.supported_rates);
		IF_DSD(	output.next_fmt = PCM; )
		output.track_start = outputbuf->writep;
		if (output.fade_mode) _checkfade(true);
		decode.new_stream = false;
		UNLOCK_O;

		if (v->channels > 2) {
			LOG_WARN("too many channels: %d", v->channels);
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
    
    void** pcm = NULL;
    int packet, n = 0;
	
    if (v->overflow) {
        n = pcm_out(&v->decoder, &pcm);
        v->overflow = n - min(n, frames);                
    } else if ((packet = get_audio_packet()) > 0) {
		n = OV(&gv, synthesis, &v->block, &v->packet);
		if (n == 0) n = OV(&gv, synthesis_blockin, &v->decoder, &v->block);
        if (n == 0) n = pcm_out(&v->decoder, &pcm);
        v->overflow = n - min(n, frames);
	} else if (!packet) {
		UNLOCK_O_direct;
		return DECODE_RUNNING;
	}

	if (n > 0) {
        ISAMPLE_T *optr = (ISAMPLE_T*) write_buf;     
        frames = min(n, frames);
        frames_t count = frames;
               
#ifndef TREMOR_ONLY
        if (!tremor) {
            if (v->channels == 2) {
                float* iptr_l = (float*) pcm[0];
                float* iptr_r = (float*) pcm[1];

                while (count--) {
                    *optr++ = ALIGN_FLOAT(*iptr_l++);
                    *optr++ = ALIGN_FLOAT(*iptr_r++);;
                }
            } else if (v->channels == 1) {
                float* iptr = pcm[0];
                while (count--) {
                    *optr++ = ALIGN_FLOAT(*iptr);
                    *optr++ = ALIGN_FLOAT(*iptr++);
                }
            }
        } else
#else
        {
            if (v->channels == 2) {
                s32_t* iptr_l = (s32_t*) pcm[0];
                s32_t* iptr_r = (s32_t*) pcm[1];

                while (count--) {
                    *optr++ = ALIGN(*iptr_l++);
                    *optr++ = ALIGN(*iptr_r++);
                }
            } else if (v->channels == 1) {
                s32_t* iptr = (s32_t*) pcm[0];
                while (count--) {
                    *optr++ = ALIGN(*iptr);
                    *optr++ = ALIGN(*iptr++);
                }
            }
        }
#endif
		// return samples to vorbis/tremor decoder
		OV(&gv, synthesis_read, &v->decoder, frames);        
		
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

		LOG_INFO("ov_read error: %d", n);
		UNLOCK_O_direct;
		return DECODE_COMPLETE;
	}

	UNLOCK_O_direct;
	return DECODE_RUNNING;
}

static void vorbis_open(u8_t size, u8_t rate, u8_t chan, u8_t endianness) {
	if (v->opened) {
		OV(&gv, block_clear, &v->block);
		OV(&gv, dsp_clear, &v->decoder);
		OV(&gv, info_clear, &v->info);        
	}
    
    v->opened = false;
	v->status = OGG_ID_HEADER;
    v->overflow = 0;

    OG(&go, stream_clear, &v->state);  
    OG(&go, sync_clear, &v->sync);
    OG(&go, stream_init, &v->state, -1);    
}

static void vorbis_close() {
	if (v->opened) {
		OV(&gv, block_clear, &v->block);
		OV(&gv, dsp_clear, &v->decoder);
        // info must be last otherwise there is memory leak (where is it said... nowhere)
		OV(&gv, info_clear, &v->info);
        // we don' t have comments to free
	}
    
    v->opened = false;

	OG(&go, stream_clear, &v->state);
	OG(&go, sync_clear, &v->sync);
}

static bool load_vorbis() {
#if !LINKALL
    char *err;
    
    void *g_handle = dlopen(LIBOGG, RTLD_NOW);
    if (!g_handle) {
        LOG_INFO("ogg dlerror: %s", dlerror());
		return false
    }
    
    void *v_handle = NULL;
#ifndef TREMOR_ONLY
	v_handle = dlopen(LIBVORBIS, RTLD_NOW);
#endif    
	if (!v_handle) {
		v_handle = dlopen(LIBTREMOR, RTLD_NOW);
		if (v_handle) {
			tremor = true;
		} else {
            dlclose(g_handle);
			LOG_INFO("vorbis/tremor dlerror: %s", dlerror());
			return false;
		}
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

	v_handle.ov_read = dlsym(handle, "ov_read");
	v_handle.ov_info = dlsym(handle, "ov_info");
	v_handle.ov_clear = dlsym(handle, "ov_clear");
	v.handle.ov_info_clear = dlsym(gv.handle, "vorbis_info_clear");
	v.handle.ov_comment_init = dlsym(gv.handle, "vorbis_comment_init");
    v.handle.ov_comment_clear = dlsym(gv.handle, "vorbis_comment_clear");
    v.handle.ov_block_init = dlsym(gv.handle, "vorbis_block_init");    
	v.handle.ov_block_clear = dlsym(gv.handle, "vorbis_block_clear");    
	v_handle.ov_open_callbacks = dlsym(handle, "ov_open_callbacks");
	
	if ((err = dlerror()) != NULL) {
		LOG_INFO("dlerror: %s", err);		
		return false;
	}
	
	LOG_INFO("loaded %s", tremor ? LIBTREMOR : LIBVORBIS);
#endif

	return true;
}

struct codec *register_vorbis(void) {
	static struct codec ret = {
		'o',          // id
		"ogg",        // types
		4096,         // min read
		20480,        // min space
		vorbis_open,  // open
		vorbis_close, // close
		vorbis_decode,// decode
	};

	if ((v = calloc(1, sizeof(struct vorbis))) == NULL) {
		return NULL;
	}

	v->opened = false;

	if (!load_vorbis()) {
		return NULL;
	}

	LOG_INFO("using vorbis to decode ogg");
	return &ret;
}
