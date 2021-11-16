/* 
 *  Squeezelite for esp32
 *
 *  (c) Sebastien 2019
 *      Philippe G. 2019, philippe_44@outlook.com
 *
 *  This software is released under the MIT License.
 *  https://opensource.org/licenses/MIT
 *
 */
 
#pragma once

#ifndef QUOTE
#define QUOTE(name) #name
#endif
#ifndef STR
#define STR(macro)  QUOTE(macro)
#endif
#define ESP_LOG_DEBUG_EVENT(tag,e) ESP_LOGD(tag,"evt: " e)
#ifndef STR_OR_ALT
#define STR_OR_ALT(str,alt) (str?str:alt)
#endif
#define ENUM_TO_STRING(g) \
    case g:     \
        return STR(g);    \
        break;
extern const char unknown_string_placeholder[];
extern const char * str_or_unknown(const char * str);
extern const char * str_or_null(const char * str);

#ifndef FREE_AND_NULL
#define FREE_AND_NULL(x) if(x) { free(x); x=NULL; }
#endif

#ifndef CASE_TO_STR
#define CASE_TO_STR(x) case x: return STR(x); break;
#endif
