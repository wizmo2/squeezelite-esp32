/* 
 *  (c) Philippe G. 20201, philippe_44@outlook.com
 *	see other copyrights below
 *
 *  This software is released under the MIT License.
 *  https://opensource.org/licenses/MIT
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "tools.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

const static char TAG[] = "tools";

/****************************************************************************************
 * UTF-8 tools
 */

// Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.
// Copyright (c) 2017 ZephRay <zephray@outlook.com>
//
// utf8to1252 - almost equivalent to iconv -f utf-8 -t windows-1252, but better

#define UTF8_ACCEPT 0
#define UTF8_REJECT 1

static const uint8_t utf8d[] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 00..1f
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 20..3f
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 40..5f
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 60..7f
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, // 80..9f
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, // a0..bf
	8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, // c0..df
	0xa,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x4,0x3,0x3, // e0..ef
	0xb,0x6,0x6,0x6,0x5,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8, // f0..ff
	0x0,0x1,0x2,0x3,0x5,0x8,0x7,0x1,0x1,0x1,0x4,0x6,0x1,0x1,0x1,0x1, // s0..s0
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,0,1,0,1,1,1,1,1,1, // s1..s2
	1,2,1,1,1,1,1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1, // s3..s4
	1,2,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,3,1,1,1,1,1,1, // s5..s6
	1,3,1,1,1,1,1,3,1,3,1,1,1,1,1,1,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // s7..s8
};

static uint32_t decode(uint32_t* state, uint32_t* codep, uint32_t byte) {
	uint32_t type = utf8d[byte];

	*codep = (*state != UTF8_ACCEPT) ?
		(byte & 0x3fu) | (*codep << 6) :
		(0xff >> type) & (byte);

	*state = utf8d[256 + *state*16 + type];
	return *state;
}

static uint8_t UNICODEtoCP1252(uint16_t chr) {
	if (chr <= 0xff)
		return (chr&0xff);
	else {
		ESP_LOGI(TAG, "some multi-byte %hx", chr);
		switch(chr) {
			case 0x20ac: return 0x80; break;
			case 0x201a: return 0x82; break;
			case 0x0192: return 0x83; break;
			case 0x201e: return 0x84; break;
			case 0x2026: return 0x85; break;
			case 0x2020: return 0x86; break;
			case 0x2021: return 0x87; break;
			case 0x02c6: return 0x88; break;
			case 0x2030: return 0x89; break;
			case 0x0160: return 0x8a; break;
			case 0x2039: return 0x8b; break;
			case 0x0152: return 0x8c; break;
			case 0x017d: return 0x8e; break;
			case 0x2018: return 0x91; break;
			case 0x2019: return 0x92; break;
			case 0x201c: return 0x93; break;
			case 0x201d: return 0x94; break;
			case 0x2022: return 0x95; break;
			case 0x2013: return 0x96; break;
			case 0x2014: return 0x97; break;
			case 0x02dc: return 0x98; break;
			case 0x2122: return 0x99; break;
			case 0x0161: return 0x9a; break;
			case 0x203a: return 0x9b; break;
			case 0x0153: return 0x9c; break;
			case 0x017e: return 0x9e; break;
			case 0x0178: return 0x9f; break;
			default: return 0x00; break;
		}
	}
}

void utf8_decode(char *src) {
	uint32_t codep = 0, state = UTF8_ACCEPT;
	char *dst = src;
			
	while (src && *src) {
		if (!decode(&state, &codep, *src++)) *dst++ = UNICODEtoCP1252(codep);
	}
	
	*dst = '\0';
}

/****************************************************************************************
 * URL tools
 */

static inline char from_hex(char ch) {
  return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

void url_decode(char *url) {
	char *p, *src = strdup(url);
	for (p = src; *src; url++) {
		*url = *src++;
		if (*url == '%') {
			*url = from_hex(*src++) << 4;
			*url |= from_hex(*src++);
		} else if (*url == '+') {
			*url = ' ';
		}
	}
	*url = '\0';
	free(p);
}

/****************************************************************************************
 * Memory tools
 */

void * malloc_init_external(size_t sz){
	void * ptr=NULL;
	ptr = heap_caps_malloc(sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if(ptr==NULL){
		ESP_LOGE(TAG,"malloc_init_external:  unable to allocate %d bytes of PSRAM!",sz);
	}
	else {
		memset(ptr,0x00,sz);
	}
	return ptr;
}

void * clone_obj_psram(void * source, size_t source_sz){
	void * ptr=NULL;
	ptr = heap_caps_malloc(source_sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if(ptr==NULL){
		ESP_LOGE(TAG,"clone_obj_psram:  unable to allocate %d bytes of PSRAM!",source_sz);
	}
	else {
		memcpy(ptr,source,source_sz);
	}
	return ptr;
}

char * strdup_psram(const char * source){
	void * ptr=NULL;
	size_t source_sz = strlen(source)+1;
	ptr = heap_caps_malloc(source_sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if(ptr==NULL){
		ESP_LOGE(TAG,"strdup_psram:  unable to allocate %d bytes of PSRAM! Cannot clone string %s",source_sz,source);
	}
	else {
		memset(ptr,0x00,source_sz);
		strcpy(ptr,source);
	}
	return ptr;
}
