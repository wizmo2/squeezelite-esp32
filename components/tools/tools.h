/* 
 *  Tools 
 *
 *      Philippe G. 2019, philippe_44@outlook.com
 *
 *  This software is released under the MIT License.
 *  https://opensource.org/licenses/MIT
 *
 */
 
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifndef QUOTE
#define QUOTE(name) #name
#endif

#ifndef STR
#define STR(macro)  QUOTE(macro)
#endif

#ifndef STR_OR_ALT
#define STR_OR_ALT(str,alt) (str?str:alt)
#endif

#ifndef STR_OR_BLANK
#define STR_OR_BLANK(p) p == NULL ? "" : p
#endif

#define ESP_LOG_DEBUG_EVENT(tag,e) ESP_LOGD(tag,"evt: " e)

#ifndef FREE_AND_NULL
#define FREE_AND_NULL(x) if(x) { free(x); x=NULL; }
#endif

#ifndef CASE_TO_STR
#define CASE_TO_STR(x) case x: return STR(x); break;
#endif

#define ENUM_TO_STRING(g) 	\
    case g:    				\
        return STR(g);    	\
        break;

void 		utf8_decode(char *src);
void 		url_decode(char *url);
void* 		malloc_init_external(size_t sz);
void* 		clone_obj_psram(void * source, size_t source_sz);
char* 		strdup_psram(const char * source);
const char* str_or_unknown(const char * str);
const char* str_or_null(const char * str);

typedef void (*http_download_cb_t)(uint8_t* data, size_t len, void *context);
void		http_download(char *url, size_t max, http_download_cb_t callback, void *context);

extern const char unknown_string_placeholder[];

#ifdef __cplusplus
}
#endif
