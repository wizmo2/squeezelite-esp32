#pragma once
#include <stdio.h>
#include <string.h>
#include "nvs.h"
#include "assert.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PARSE_PARAM(S,P,C,V) do {												\
	char *__p;																	\
	if ((__p = strcasestr(S, P)) && (__p = strchr(__p, C))) V = atoi(__p+1); 	\
} while (0)

#define PARSE_PARAM_FLOAT(S,P,C,V) do {												\
	char *__p;																	\
	if ((__p = strcasestr(S, P)) && (__p = strchr(__p, C))) V = atof(__p+1); 	\
} while (0)

#define PARSE_PARAM_STR(S,P,C,V,I) do {						\
	char *__p;                                              \
	if ((__p = strstr(S, P)) && (__p = strchr(__p, C))) {	\
		while (*++__p == ' ');								\
		sscanf(__p,"%" #I "[^,]", V);						\
	}   													\
} while (0)

#define DECLARE_SET_DEFAULT(t) void config_set_default_## t (const char *key, t  value);
#define DECLARE_GET_NUM(t) esp_err_t config_get_## t (const char *key, t *  value);
#ifndef FREE_RESET
#define FREE_RESET(p) if(p!=NULL) { free(p); p=NULL; }
#endif

DECLARE_SET_DEFAULT(uint8_t);
DECLARE_SET_DEFAULT(uint16_t);
DECLARE_SET_DEFAULT(uint32_t);
DECLARE_SET_DEFAULT(int8_t);
DECLARE_SET_DEFAULT(int16_t);
DECLARE_SET_DEFAULT(int32_t);
DECLARE_GET_NUM(uint8_t);
DECLARE_GET_NUM(uint16_t);
DECLARE_GET_NUM(uint32_t);
DECLARE_GET_NUM(int8_t);
DECLARE_GET_NUM(int16_t);
DECLARE_GET_NUM(int32_t);

bool config_has_changes();
void config_commit_to_nvs();
void config_start_timer();
void config_init();
void * config_alloc_get_default(nvs_type_t type, const char *key, void * default_value, size_t blob_size);
void * config_alloc_get_str(const char *key, char *lead, char *fallback);
cJSON * config_alloc_get_cjson(const char *key);
esp_err_t config_set_cjson_str_and_free(const char *key, cJSON *value);
void config_get_uint16t_from_str(const char *key, uint16_t *value, uint16_t default_value);
void config_delete_key(const char *key);
void config_set_default(nvs_type_t type, const char *key, const void * default_value, size_t blob_size);
void * config_alloc_get(nvs_type_t nvs_type, const char *key) ;
bool wait_for_commit();
char * config_alloc_get_json(bool bFormatted);
esp_err_t config_set_value(nvs_type_t nvs_type, const char *key, const void * value);
nvs_type_t  config_get_item_type(cJSON * entry);
void * config_safe_alloc_get_entry_value(nvs_type_t nvs_type, cJSON * entry);
cJSON* cjson_update_number(cJSON** root, const char* key, int value);
cJSON* cjson_update_string(cJSON** root, const char* key, const char* value);
#ifdef __cplusplus
}
#endif

