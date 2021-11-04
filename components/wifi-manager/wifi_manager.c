/*
Copyright (c) 2017-2019 Tony Pottier

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

@file wifi_manager.c
@author Tony Pottier
@brief Defines all functions necessary for esp32 to connect to a wifi/scan wifis

Contains the freeRTOS task and all necessary support

@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#include "wifi_manager.h"
#include "platform_esp32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "dns_server.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <esp_event.h>
#include "esp_event_loop.h"
#include "tcpip_adapter.h"
// IDF-V4++ #include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "mdns.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/ip4_addr.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "cJSON.h"
#include "platform_config.h"
#include "trace.h"
#include "cmd_system.h"
#include "messaging.h"
#include "bt_app_core.h"

#include "http_server_handlers.h"
#include "monitor.h"
#include "globdefs.h"


#ifndef CONFIG_SQUEEZELITE_ESP32_RELEASE_URL
#pragma message "Defaulting release url"
#define CONFIG_SQUEEZELITE_ESP32_RELEASE_URL "https://github.com/sle118/squeezelite-esp32/releases"
#endif

#define STR_OR_BLANK(p) p==NULL?"":p
BaseType_t wifi_manager_task;

/* objects used to manipulate the main queue of events */
QueueHandle_t wifi_manager_queue;
SemaphoreHandle_t wifi_manager_json_mutex = NULL;
SemaphoreHandle_t wifi_manager_sta_ip_mutex = NULL;
char *wifi_manager_sta_ip = NULL;
#define STA_IP_LEN sizeof(char) * IP4ADDR_STRLEN_MAX
uint16_t ap_num = MAX_AP_NUM;
wifi_ap_record_t *accessp_records=NULL;
cJSON * accessp_cjson=NULL;
char *ip_info_json = NULL;
char * release_url=NULL;
cJSON * ip_info_cjson=NULL;
wifi_config_t* wifi_manager_config_sta = NULL;
static void	(*chained_notify)(in_addr_t, u16_t, u16_t);	
static int32_t total_connected_time=0;
static int64_t last_connected=0;
static uint16_t num_disconnect=0;
static char lms_server_ip[IP4ADDR_STRLEN_MAX]={0};
static uint16_t lms_server_port=0;
static uint16_t lms_server_cport=0;

void (**cb_ptr_arr)(void*) = NULL;

/* @brief tag used for ESP serial console messages */
static const char TAG[] = "wifi_manager";

/* @brief task handle for the main wifi_manager task */
static TaskHandle_t task_wifi_manager = NULL;

#define STA_POLLING_MIN	(15*1000)
#define STA_POLLING_MAX	(10*60*1000)

/**
 * The actual WiFi settings in use
 */
//struct wifi_settings_t wifi_settings = {
//	.sta_only = DEFAULT_STA_ONLY,
//	.sta_power_save = DEFAULT_STA_POWER_SAVE,
//	.sta_static_ip = 0
//};


/* wifi scanner config */
wifi_scan_config_t scan_config = {
	.ssid = 0,
	.bssid = 0,
	.channel = 0,
	.show_hidden = true
};


const char wifi_manager_nvs_namespace[] = "config";

EventGroupHandle_t wifi_manager_event_group;

/* @brief indicate that the ESP32 is currently connected. */
const int WIFI_MANAGER_WIFI_CONNECTED_BIT = BIT0;

const int WIFI_MANAGER_AP_STA_CONNECTED_BIT = BIT1;

/* @brief Set automatically once the SoftAP is started */
const int WIFI_MANAGER_AP_STARTED_BIT = BIT2;

/* @brief When set, means a client requested to connect to an access point.*/
const int WIFI_MANAGER_REQUEST_STA_CONNECT_BIT = BIT3;

/* @brief This bit is set automatically as soon as a connection was lost */
const int WIFI_MANAGER_STA_DISCONNECT_BIT = BIT4;

/* @brief When set, means the wifi manager attempts to restore a previously saved connection at startup. */
const int WIFI_MANAGER_REQUEST_RESTORE_STA_BIT = BIT5;

/* @brief When set, means a client requested to disconnect from currently connected AP. */
const int WIFI_MANAGER_REQUEST_WIFI_DISCONNECT_BIT = BIT6;

/* @brief When set, means a scan is in progress */
const int WIFI_MANAGER_SCAN_BIT = BIT7;

/* @brief When set, means user requested for a disconnect */
const int WIFI_MANAGER_REQUEST_DISCONNECT_BIT = BIT8;

/* @brief When set, means user requested connecting to a new network and it failed */
const int WIFI_MANAGER_REQUEST_STA_CONNECT_FAILED_BIT = BIT9;


char * get_disconnect_code_desc(uint8_t reason){
	switch (reason) {
		case 1	: return "UNSPECIFIED"; break;
		case 2	: return "AUTH_EXPIRE"; break;
		case 3	: return "AUTH_LEAVE"; break;
		case 4	: return "ASSOC_EXPIRE"; break;
		case 5	: return "ASSOC_TOOMANY"; break;
		case 6	: return "NOT_AUTHED"; break;
		case 7	: return "NOT_ASSOCED"; break;
		case 8	: return "ASSOC_LEAVE"; break;
		case 9	: return "ASSOC_NOT_AUTHED"; break;
		case 10	: return "DISASSOC_PWRCAP_BAD"; break;
		case 11	: return "DISASSOC_SUPCHAN_BAD"; break;
		case 12	: return "<n/a>"; break;
		case 13	: return "IE_INVALID"; break;
		case 14	: return "MIC_FAILURE"; break;
		case 15	: return "4WAY_HANDSHAKE_TIMEOUT"; break;
		case 16	: return "GROUP_KEY_UPDATE_TIMEOUT"; break;
		case 17	: return "IE_IN_4WAY_DIFFERS"; break;
		case 18	: return "GROUP_CIPHER_INVALID"; break;
		case 19	: return "PAIRWISE_CIPHER_INVALID"; break;
		case 20	: return "AKMP_INVALID"; break;
		case 21	: return "UNSUPP_RSN_IE_VERSION"; break;
		case 22	: return "INVALID_RSN_IE_CAP"; break;
		case 23	: return "802_1X_AUTH_FAILED"; break;
		case 24	: return "CIPHER_SUITE_REJECTED"; break;
		case 200	: return "BEACON_TIMEOUT"; break;
		case 201	: return "NO_AP_FOUND"; break;
		case 202	: return "AUTH_FAIL"; break;
		case 203	: return "ASSOC_FAIL"; break;
		case 204	: return "HANDSHAKE_TIMEOUT"; break;
		default: return "UNKNOWN"; break;
	}
	return "";
}
void wifi_manager_update_status(){
	wifi_manager_send_message(ORDER_UPDATE_STATUS,NULL);
}
void set_host_name(){
	esp_err_t err;
	ESP_LOGD(TAG, "Retrieving host name from nvs");
	char * host_name = (char * )config_alloc_get(NVS_TYPE_STR, "host_name");
	if(host_name ==NULL){
		ESP_LOGE(TAG,   "Could not retrieve host name from nvs");
	}
	else {
		ESP_LOGD(TAG,  "Setting host name to : %s",host_name);
		if((err=tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, host_name)) !=ESP_OK){
			ESP_LOGE(TAG,  "Unable to set host name. Error: %s",esp_err_to_name(err));
		}
//		if((err=tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_AP, host_name)) !=ESP_OK){
//			ESP_LOGE(TAG,  "Unable to set host name. Error: %s",esp_err_to_name(err));
//		}
		free(host_name);
	}

}

bool isGroupBitSet(uint8_t bit){
	EventBits_t uxBits= xEventGroupGetBits(wifi_manager_event_group);
	return (uxBits & bit);
}

void wifi_manager_scan_async(){
	wifi_manager_send_message(ORDER_START_WIFI_SCAN, NULL);
}

void wifi_manager_disconnect_async(){
	wifi_manager_send_message(ORDER_DISCONNECT_STA, NULL);
	//xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_WIFI_DISCONNECT_BIT); TODO: delete
}

void wifi_manager_reboot_ota(char * url){
	if(url == NULL){
		wifi_manager_send_message(ORDER_RESTART_OTA, NULL);
	}
	else {
		wifi_manager_send_message(ORDER_RESTART_OTA_URL,strdup(url) );
	}

}

void wifi_manager_reboot(reboot_type_t rtype){
	switch (rtype) {
	case OTA:
		wifi_manager_send_message(ORDER_RESTART_OTA, NULL);
		break;
	case RECOVERY:
		wifi_manager_send_message(ORDER_RESTART_RECOVERY, NULL);
		break;
	case RESTART:
		wifi_manager_send_message(ORDER_RESTART, NULL);
		break;
		default:
			ESP_LOGE(TAG,"Unknown reboot type %d", rtype);
			break;
	}
	wifi_manager_send_message(ORDER_DISCONNECT_STA, NULL);
	//xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_WIFI_DISCONNECT_BIT); TODO: delete
}

void wifi_manager_init_wifi(){
	/* event handler and event group for the wifi driver */
	ESP_LOGD(TAG,   "Initializing wifi.  Creating event group");
	wifi_manager_event_group = xEventGroupCreate();
	// Now Initialize the Wifi Stack
	ESP_LOGD(TAG,   "Initializing wifi. Initializing tcp_ip adapter");
    tcpip_adapter_init();
    ESP_LOGD(TAG,   "Initializing wifi. Creating the default event loop");
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_LOGD(TAG,   "Initializing wifi. Getting default wifi configuration");
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_LOGD(TAG,   "Initializing wifi. Initializing wifi. ");
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_LOGD(TAG,   "Initializing wifi. Calling register handlers");
    wifi_manager_register_handlers();
    ESP_LOGD(TAG,   "Initializing wifi. Setting WiFi storage as RAM");
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_LOGD(TAG,   "Initializing wifi. Setting WiFi mode to WIFI_MODE_NULL");
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_NULL) );
    ESP_LOGD(TAG,   "Initializing wifi. Starting wifi");
    ESP_ERROR_CHECK( esp_wifi_start() );

    taskYIELD();
    ESP_LOGD(TAG,   "Initializing wifi. done");
}

void set_lms_server_details(in_addr_t ip, u16_t hport, u16_t cport){
	strncpy(lms_server_ip,inet_ntoa(ip),sizeof(lms_server_ip));
	lms_server_ip[sizeof(lms_server_ip)-1]='\0';
	ESP_LOGI(TAG,"LMS IP: %s, hport: %d, cport: %d",lms_server_ip, hport, cport);
	lms_server_port = hport;
	lms_server_cport = cport;

}

static void connect_notify(in_addr_t ip, u16_t hport, u16_t cport) {
	set_lms_server_details(ip,hport,cport);
	if (chained_notify) (*chained_notify)(ip, hport, cport);
	wifi_manager_update_status();
}

static void polling_STA(void* timer_id) {
	wifi_manager_send_message(ORDER_CONNECT_STA, (void*)CONNECTION_REQUEST_AUTO_RECONNECT);
}

void wifi_manager_start(){


	/* memory allocation */
	ESP_LOGD(TAG,   "wifi_manager_start.  Creating message queue");
	wifi_manager_queue = xQueueCreate( 3, sizeof( queue_message) );
	ESP_LOGD(TAG,   "wifi_manager_start.  Creating mutexes");
	wifi_manager_json_mutex = xSemaphoreCreateMutex();
	wifi_manager_sta_ip_mutex = xSemaphoreCreateMutex();

	ESP_LOGD(TAG,   "wifi_manager_start.  Creating access point json structure");

	accessp_cjson = NULL;
	accessp_cjson = wifi_manager_clear_ap_list_json(&accessp_cjson);
	ip_info_json = NULL;
	ESP_LOGD(TAG,   "wifi_manager_start.  Creating status jcon structure");
	ip_info_cjson = wifi_manager_clear_ip_info_json(&ip_info_cjson);

	ESP_LOGD(TAG,   "wifi_manager_start.  Allocating memory for wifi configuration structure");
	wifi_manager_config_sta = (wifi_config_t*)malloc(sizeof(wifi_config_t));
	memset(wifi_manager_config_sta, 0x00, sizeof(wifi_config_t));
//	memset(&wifi_settings, 0x00, sizeof(wifi_settings));

	ESP_LOGD(TAG,   "wifi_manager_start.  Allocating memory for callback functions registration");
	cb_ptr_arr = malloc(  sizeof(   sizeof( void (*)( void* ) )) * MESSAGE_CODE_COUNT);
	for(int i=0; i<MESSAGE_CODE_COUNT; i++){
		cb_ptr_arr[i] = NULL;
	}

	ESP_LOGD(TAG,   "About to set the STA IP String to 0.0.0.0");
	wifi_manager_sta_ip = (char*)malloc(STA_IP_LEN);
	wifi_manager_safe_update_sta_ip_string(NULL);

	ESP_LOGD(TAG,   "Getting release url ");
	char * release_url = (char * )config_alloc_get_default(NVS_TYPE_STR, "release_url", QUOTE(CONFIG_SQUEEZELITE_ESP32_RELEASE_URL), 0);
	if(release_url == NULL){
		ESP_LOGE(TAG,  "Unable to retrieve the release url from nvs");
	}
	else {
		ESP_LOGD(TAG,   "Found release url %s", release_url);
	}
	chained_notify = server_notify;
	server_notify = connect_notify;
	ESP_LOGD(TAG,   "About to call init wifi");
	wifi_manager_init_wifi();

	/* start wifi manager task */
	ESP_LOGD(TAG,   "Creating wifi manager task");
	wifi_manager_task= xTaskCreate(&wifi_manager, "wifi_manager", 4096, NULL, WIFI_MANAGER_TASK_PRIORITY, &task_wifi_manager);
}


esp_err_t wifi_manager_save_sta_config(){
	nvs_handle handle;
	esp_err_t esp_err;
	ESP_LOGD(TAG,   "About to save config to flash");

	if(wifi_manager_config_sta){
		esp_err = nvs_open(wifi_manager_nvs_namespace, NVS_READWRITE, &handle);
		if (esp_err != ESP_OK) {
			ESP_LOGE(TAG,  "Unable to open name namespace %s. Error %s", wifi_manager_nvs_namespace, esp_err_to_name(esp_err));
			return esp_err;
		}

		esp_err = nvs_set_blob(handle, "ssid", wifi_manager_config_sta->sta.ssid, sizeof(wifi_manager_config_sta->sta.ssid));
		if (esp_err != ESP_OK) {
			ESP_LOGE(TAG,  "Unable to save ssid in name namespace %s. Error %s", wifi_manager_nvs_namespace, esp_err_to_name(esp_err));
			return esp_err;
		}

		esp_err = nvs_set_blob(handle, "password", wifi_manager_config_sta->sta.password, sizeof(wifi_manager_config_sta->sta.password));
		if (esp_err != ESP_OK) {
			ESP_LOGE(TAG,  "Unable to save password in name namespace %s. Error %s", wifi_manager_nvs_namespace, esp_err_to_name(esp_err));
			return esp_err;
		}

//		esp_err = nvs_set_blob(handle, "settings", &wifi_settings, sizeof(wifi_settings));
//		if (esp_err != ESP_OK) {
//			ESP_LOGE(TAG,  "Unable to save wifi_settings in name namespace %s. Error %s", wifi_manager_nvs_namespace, esp_err_to_name(esp_err));
//			return esp_err;
//		}

		esp_err = nvs_commit(handle);
		if (esp_err != ESP_OK) {
			ESP_LOGE(TAG,  "Unable to commit changes. Error %s", esp_err_to_name(esp_err));
			messaging_post_message(MESSAGING_ERROR,MESSAGING_CLASS_SYSTEM,"Unable to save wifi credentials. %s",esp_err_to_name(esp_err));
			return esp_err;
		}
		nvs_close(handle);

		ESP_LOGD(TAG,   "wifi_manager_wrote wifi_sta_config: ssid:%s password:%s",wifi_manager_config_sta->sta.ssid,wifi_manager_config_sta->sta.password);
	}

	return ESP_OK;
}

bool wifi_manager_fetch_wifi_sta_config(){
	nvs_handle handle;
	esp_err_t esp_err;

	ESP_LOGD(TAG,  "Fetching wifi sta config.");
	esp_err=nvs_open(wifi_manager_nvs_namespace, NVS_READONLY, &handle);
	if(esp_err == ESP_OK){
		if(wifi_manager_config_sta == NULL){
			ESP_LOGD(TAG,  "Allocating memory for structure.");
			wifi_manager_config_sta = (wifi_config_t*)malloc(sizeof(wifi_config_t));
		}
		memset(wifi_manager_config_sta, 0x00, sizeof(wifi_config_t));

		/* ssid */
		ESP_LOGD(TAG,  "Fetching value for ssid.");
		size_t sz = sizeof(wifi_manager_config_sta->sta.ssid);
		uint8_t *buff = (uint8_t*)malloc(sizeof(uint8_t) * sz);
		memset(buff,0x00,sizeof(uint8_t) * sz);
		esp_err = nvs_get_blob(handle, "ssid", buff, &sz);
		if(esp_err != ESP_OK){
			ESP_LOGD(TAG,  "No ssid found in nvs.");
			FREE_AND_NULL(buff);
			nvs_close(handle);
			return false;
		}
		memcpy(wifi_manager_config_sta->sta.ssid, buff, sizeof(wifi_manager_config_sta->sta.ssid));
		FREE_AND_NULL(buff);
		ESP_LOGD(TAG,   "wifi_manager_fetch_wifi_sta_config: ssid:%s ",wifi_manager_config_sta->sta.ssid);

				/* password */
		sz = sizeof(wifi_manager_config_sta->sta.password);
		buff = (uint8_t*)malloc(sizeof(uint8_t) * sz);
		memset(buff,0x00,sizeof(uint8_t) * sz);
		esp_err = nvs_get_blob(handle, "password", buff, &sz);
		if(esp_err != ESP_OK){
			// Don't take this as an error. This could be an opened access point?
			ESP_LOGW(TAG,  "No wifi password found in nvs");
		}
		else {
			memcpy(wifi_manager_config_sta->sta.password, buff, sizeof(wifi_manager_config_sta->sta.password));
			ESP_LOGD(TAG,   "wifi_manager_fetch_wifi_sta_config: password:%s",wifi_manager_config_sta->sta.password);
		}
		FREE_AND_NULL(buff);
		nvs_close(handle);

		return wifi_manager_config_sta->sta.ssid[0] != '\0';
	}
	else{
		ESP_LOGW(TAG,  "wifi manager has no previous configuration. %s",esp_err_to_name(esp_err));
		return false;
	}

}

cJSON * wifi_manager_get_new_json(cJSON **old){
	ESP_LOGV(TAG,  "wifi_manager_get_new_json called");
	cJSON * root=*old;
	if(root!=NULL){
	    cJSON_Delete(root);
	    *old=NULL;
	}
	ESP_LOGV(TAG,  "wifi_manager_get_new_json done");
	 return cJSON_CreateObject();
}
cJSON * wifi_manager_get_new_array_json(cJSON **old){
	ESP_LOGV(TAG,  "wifi_manager_get_new_array_json called");
	cJSON * root=*old;
	if(root!=NULL){
	    cJSON_Delete(root);
	    *old=NULL;
	}
	ESP_LOGV(TAG,  "wifi_manager_get_new_array_json done");
	return cJSON_CreateArray();
}
void wifi_manager_update_basic_info(){
	if(wifi_manager_lock_json_buffer( portMAX_DELAY )){

		monitor_gpio_t *mgpio= get_jack_insertion_gpio(); 
		
		cJSON * voltage = cJSON_GetObjectItemCaseSensitive(ip_info_cjson, "Voltage");
		if(voltage){
			cJSON_SetNumberValue(voltage,	battery_value_svc());
		}
		cJSON * bt_status = cJSON_GetObjectItemCaseSensitive(ip_info_cjson, "bt_status");
		if(bt_status){
			cJSON_SetNumberValue(bt_status,	bt_app_source_get_a2d_state());
		}
		cJSON * bt_sub_status = cJSON_GetObjectItemCaseSensitive(ip_info_cjson, "bt_sub_status");
		if(bt_sub_status){
			cJSON_SetNumberValue(bt_sub_status,	bt_app_source_get_media_state());
		}
		cJSON * jack = cJSON_GetObjectItemCaseSensitive(ip_info_cjson, "Jack");
		if(jack){
			jack->type=mgpio->gpio>=0 && jack_inserted_svc()?cJSON_True:cJSON_False;
		}
		cJSON * disconnect_count = cJSON_GetObjectItemCaseSensitive(ip_info_cjson, "disconnect_count");
		if(disconnect_count){
			cJSON_SetNumberValue(disconnect_count,	num_disconnect);
		}
		cJSON * avg_conn_time = cJSON_GetObjectItemCaseSensitive(ip_info_cjson, "avg_conn_time");
		if(avg_conn_time){
			cJSON_SetNumberValue(avg_conn_time,	num_disconnect>0?(total_connected_time/num_disconnect):0);
		}	
		if(lms_server_cport>0){
			cJSON * value = cJSON_GetObjectItemCaseSensitive(ip_info_cjson, "lms_cport");
			if(value){
				cJSON_SetNumberValue(value,lms_server_cport);
			}			
			else {
				cJSON_AddNumberToObject(ip_info_cjson,"lms_cport",lms_server_cport);
			}
		}

		if(lms_server_port>0){
			cJSON * value = cJSON_GetObjectItemCaseSensitive(ip_info_cjson, "lms_port");
			if(value){
				cJSON_SetNumberValue(value,lms_server_port);
			}			
			else {
				cJSON_AddNumberToObject(ip_info_cjson,"lms_port",lms_server_port);
			}
		}


		if(strlen(lms_server_ip) >0){
			cJSON * value = cJSON_GetObjectItemCaseSensitive(ip_info_cjson, "lms_ip");
			if(!value){
				// only create if it does not exist. Since we're creating a reference 
				// to a char buffer, updates to cJSON aren't needed
				cJSON_AddItemToObject(ip_info_cjson, "lms_ip", cJSON_CreateStringReference(lms_server_ip));
			}			
		}		
		wifi_manager_unlock_json_buffer();
	}
}
cJSON * wifi_manager_get_basic_info(cJSON **old){
	monitor_gpio_t *mgpio= get_jack_insertion_gpio(); 
	const esp_app_desc_t* desc = esp_ota_get_app_description();
	ESP_LOGV(TAG,  "wifi_manager_get_basic_info called");
	cJSON *root = wifi_manager_get_new_json(old);
	cJSON_AddItemToObject(root, "project_name", cJSON_CreateString(desc->project_name));
	#ifdef CONFIG_FW_PLATFORM_NAME
		cJSON_AddItemToObject(root, "platform_name", cJSON_CreateString(CONFIG_FW_PLATFORM_NAME));
	#endif
	cJSON_AddItemToObject(root, "version", cJSON_CreateString(desc->version));
	if(release_url !=NULL) cJSON_AddItemToObject(root, "release_url", cJSON_CreateString(release_url));
	cJSON_AddNumberToObject(root,"recovery",	is_recovery_running?1:0);
	cJSON_AddBoolToObject(root, "Jack", mgpio->gpio>=0 && jack_inserted_svc() );
	cJSON_AddNumberToObject(root,"Voltage",	battery_value_svc());
	cJSON_AddNumberToObject(root,"disconnect_count", num_disconnect	);
	cJSON_AddNumberToObject(root,"avg_conn_time", num_disconnect>0?(total_connected_time/num_disconnect):0	);
	cJSON_AddNumberToObject(root,"bt_status", bt_app_source_get_a2d_state());
	cJSON_AddNumberToObject(root,"bt_sub_status", bt_app_source_get_media_state());
#if CONFIG_I2C_LOCKED
	cJSON_AddTrueToObject(root, "is_i2c_locked");
#else
	cJSON_AddFalseToObject(root, "is_i2c_locked");
#endif

	ESP_LOGV(TAG,  "wifi_manager_get_basic_info done");
	return root;
}
cJSON * wifi_manager_clear_ip_info_json(cJSON **old){
	ESP_LOGV(TAG,  "wifi_manager_clear_ip_info_json called");
	cJSON *root = wifi_manager_get_basic_info(old);
	ESP_LOGV(TAG,  "wifi_manager_clear_ip_info_json done");
 	 return root;
}
cJSON * wifi_manager_clear_ap_list_json(cJSON **old){
	ESP_LOGV(TAG,  "wifi_manager_clear_ap_list_json called");
	cJSON *root = wifi_manager_get_new_array_json(old);
	ESP_LOGV(TAG,  "wifi_manager_clear_ap_list_json done");
 	return root;
}



void wifi_manager_generate_ip_info_json(update_reason_code_t update_reason_code){
	ESP_LOGD(TAG,  "wifi_manager_generate_ip_info_json called");
	wifi_config_t *config = wifi_manager_get_wifi_sta_config();
	ip_info_cjson = wifi_manager_get_basic_info(&ip_info_cjson);

	cJSON_AddNumberToObject(ip_info_cjson, "urc", update_reason_code);
	if(config){
		if(update_reason_code == UPDATE_CONNECTION_OK || update_reason_code == UPDATE_LOST_CONNECTION || update_reason_code == UPDATE_FAILED_ATTEMPT){
			cJSON_AddItemToObject(ip_info_cjson, "ssid", cJSON_CreateString((char *)config->sta.ssid));
		}
		if(update_reason_code == UPDATE_CONNECTION_OK){
			/* rest of the information is copied after the ssid */
			tcpip_adapter_ip_info_t ip_info;
			ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
			cJSON_AddItemToObject(ip_info_cjson, "ip", cJSON_CreateString(ip4addr_ntoa((ip4_addr_t *)&ip_info.ip)));
			cJSON_AddItemToObject(ip_info_cjson, "netmask", cJSON_CreateString(ip4addr_ntoa((ip4_addr_t *)&ip_info.netmask)));
			cJSON_AddItemToObject(ip_info_cjson, "gw", cJSON_CreateString(ip4addr_ntoa((ip4_addr_t *)&ip_info.gw)));
			wifi_ap_record_t ap;
			esp_wifi_sta_get_ap_info(&ap);
			cJSON_AddItemToObject(ip_info_cjson, "rssi", cJSON_CreateNumber(ap.rssi));
		}
	}

	ESP_LOGV(TAG,  "wifi_manager_generate_ip_info_json done");
}
#define LOCAL_MAC_SIZE 20
char * get_mac_string(uint8_t mac[6]){

	char * macStr=malloc(LOCAL_MAC_SIZE);
	memset(macStr, 0x00, LOCAL_MAC_SIZE);
	snprintf(macStr, LOCAL_MAC_SIZE,MACSTR, MAC2STR(mac));
	return macStr;

}
void wifi_manager_generate_access_points_json(cJSON ** ap_list){
	*ap_list = wifi_manager_get_new_array_json(ap_list);

	if(*ap_list==NULL) return;
	for(int i=0; i<ap_num;i++){
		cJSON * ap = cJSON_CreateObject();
		if(ap == NULL) {
			ESP_LOGE(TAG,  "Unable to allocate memory for access point entry #%d",i);
			return;
		}
		cJSON * radio = cJSON_CreateObject();
		if(radio == NULL) {
			ESP_LOGE(TAG,  "Unable to allocate memory for access point entry #%d",i);
			cJSON_Delete(ap);
			return;
		}
		wifi_ap_record_t ap_rec = accessp_records[i];
		cJSON_AddNumberToObject(ap, "chan", ap_rec.primary);
		cJSON_AddNumberToObject(ap, "rssi", ap_rec.rssi);
		cJSON_AddNumberToObject(ap, "auth", ap_rec.authmode);
		cJSON_AddItemToObject(ap, "ssid", cJSON_CreateString((char *)ap_rec.ssid));

		char * bssid = get_mac_string(ap_rec.bssid);
		cJSON_AddItemToObject(ap, "bssid", cJSON_CreateString(STR_OR_BLANK(bssid)));
		FREE_AND_NULL(bssid);
		cJSON_AddNumberToObject(radio, "b", ap_rec.phy_11b?1:0);
		cJSON_AddNumberToObject(radio, "g", ap_rec.phy_11g?1:0);
		cJSON_AddNumberToObject(radio, "n", ap_rec.phy_11n?1:0);
		cJSON_AddNumberToObject(radio, "low_rate", ap_rec.phy_lr?1:0);
		cJSON_AddItemToObject(ap,"radio", radio);
		cJSON_AddItemToArray(*ap_list, ap);
		char * ap_json = cJSON_PrintUnformatted(ap);
		if(ap_json!=NULL){
			ESP_LOGD(TAG,  "New access point found: %s", ap_json);
			free(ap_json);
		}
	}
	char * ap_list_json = cJSON_PrintUnformatted(*ap_list);
	if(ap_list_json!=NULL){
		ESP_LOGV(TAG,  "Full access point list: %s", ap_list_json);
		free(ap_list_json);
	}

}

bool wifi_manager_lock_sta_ip_string(TickType_t xTicksToWait){
	if(wifi_manager_sta_ip_mutex){
		if( xSemaphoreTake( wifi_manager_sta_ip_mutex, xTicksToWait ) == pdTRUE ) {
			return true;
		}
		else{
			return false;
		}
	}
	else{
		return false;
	}

}

void wifi_manager_unlock_sta_ip_string(){
	xSemaphoreGive( wifi_manager_sta_ip_mutex );
}

void wifi_manager_safe_update_sta_ip_string(struct ip4_addr * ip4){
	if(wifi_manager_lock_sta_ip_string(portMAX_DELAY)){
		strcpy(wifi_manager_sta_ip, ip4!=NULL?ip4addr_ntoa(ip4):"0.0.0.0");
		ESP_LOGD(TAG,   "Set STA IP String to: %s", wifi_manager_sta_ip);
		wifi_manager_unlock_sta_ip_string();
	}
}

char* wifi_manager_get_sta_ip_string(){
	return wifi_manager_sta_ip;
}

bool wifi_manager_lock_json_buffer(TickType_t xTicksToWait){
	ESP_LOGV(TAG,  "Locking json buffer");
	if(wifi_manager_json_mutex){
		if( xSemaphoreTake( wifi_manager_json_mutex, xTicksToWait ) == pdTRUE ) {
			ESP_LOGV(TAG,  "Json buffer locked!");
			return true;
		}
		else{
			ESP_LOGE(TAG,  "Semaphore take failed. Unable to lock json buffer mutex");
			return false;
		}
	}
	else{
		ESP_LOGV(TAG,  "Unable to lock json buffer mutex");
		return false;
	}

}

void wifi_manager_unlock_json_buffer(){
	ESP_LOGV(TAG,  "Unlocking json buffer!");
	xSemaphoreGive( wifi_manager_json_mutex );
}

char* wifi_manager_alloc_get_ap_list_json(){
	return cJSON_PrintUnformatted(accessp_cjson);
}


static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){

    if(event_base== WIFI_EVENT){
		switch(event_id) {
			case WIFI_EVENT_WIFI_READY:
				ESP_LOGD(TAG,   "WIFI_EVENT_WIFI_READY");
				break;

			case WIFI_EVENT_SCAN_DONE:
				ESP_LOGD(TAG,   "WIFI_EVENT_SCAN_DONE");
				xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_SCAN_BIT);
				wifi_manager_send_message(EVENT_SCAN_DONE, NULL);
				break;

			case WIFI_EVENT_STA_AUTHMODE_CHANGE:
				ESP_LOGD(TAG,   "WIFI_EVENT_STA_AUTHMODE_CHANGE");
//		        	structwifi_event_sta_authmode_change_t
//		        	Argument structure for WIFI_EVENT_STA_AUTHMODE_CHANGE event
//
//		        	Public Members
//
//		        	wifi_auth_mode_told_mode
//		        	the old auth mode of AP
//
//		        	wifi_auth_mode_tnew_mode
//		        	the new auth mode of AP
				break;


			case WIFI_EVENT_AP_START:
				ESP_LOGD(TAG,   "WIFI_EVENT_AP_START");
				xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_AP_STARTED_BIT);
				break;

			case WIFI_EVENT_AP_STOP:
				ESP_LOGD(TAG,  "WIFI_EVENT_AP_STOP");
				break;

			case WIFI_EVENT_AP_PROBEREQRECVED:{
//		        	wifi_event_ap_probe_req_rx_t
//		        	Argument structure for WIFI_EVENT_AP_PROBEREQRECVED event
//
//		        	Public Members
//
//		        	int rssi
//		        	Received probe request signal strength
//
//		        	uint8_t mac[6]
//		        	MAC address of the station which send probe request

				wifi_event_ap_probe_req_rx_t * s =(wifi_event_ap_probe_req_rx_t*)event_data;
				char * mac = get_mac_string(s->mac);
				ESP_LOGD(TAG,  "WIFI_EVENT_AP_PROBEREQRECVED. RSSI: %d, MAC: %s",s->rssi, STR_OR_BLANK(mac));
				FREE_AND_NULL(mac);
			}
				break;
			case WIFI_EVENT_STA_WPS_ER_SUCCESS:
				ESP_LOGD(TAG,  "WIFI_EVENT_STA_WPS_ER_SUCCESS");
				break;
			case WIFI_EVENT_STA_WPS_ER_FAILED:
				ESP_LOGD(TAG,  "WIFI_EVENT_STA_WPS_ER_FAILED");
				break;
			case WIFI_EVENT_STA_WPS_ER_TIMEOUT:
				ESP_LOGD(TAG,  "WIFI_EVENT_STA_WPS_ER_TIMEOUT");
				break;
			case WIFI_EVENT_STA_WPS_ER_PIN:
				ESP_LOGD(TAG,  "WIFI_EVENT_STA_WPS_ER_PIN");
				break;
			case WIFI_EVENT_AP_STACONNECTED:{ /* a user disconnected from the SoftAP */
				wifi_event_ap_staconnected_t * stac = (wifi_event_ap_staconnected_t *)event_data;
				char * mac = get_mac_string(stac->mac);
				ESP_LOGD(TAG,   "WIFI_EVENT_AP_STACONNECTED. aid: %d, mac: %s",stac->aid,STR_OR_BLANK(mac));
				FREE_AND_NULL(mac);
				xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_AP_STA_CONNECTED_BIT);
			}
				break;
			case WIFI_EVENT_AP_STADISCONNECTED:
				ESP_LOGD(TAG,   "WIFI_EVENT_AP_STADISCONNECTED");
				xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_AP_STA_CONNECTED_BIT);
				break;

			case WIFI_EVENT_STA_START:
				ESP_LOGD(TAG,   "WIFI_EVENT_STA_START");
				break;

			case WIFI_EVENT_STA_STOP:
				ESP_LOGD(TAG,   "WIFI_EVENT_STA_STOP");
				break;

			case WIFI_EVENT_STA_CONNECTED:{
//		    		structwifi_event_sta_connected_t
//		    		Argument structure for WIFI_EVENT_STA_CONNECTED event
//
//		    		Public Members
//
//		    		uint8_t ssid[32]
//		    		SSID of connected AP
//
//		    		uint8_t ssid_len
//		    		SSID length of connected AP
//
//		    		uint8_t bssid[6]
//		    		BSSID of connected AP
//
//		    		uint8_t channel
//		    		channel of connected AP
//
//		    		wifi_auth_mode_tauthmode
//		    		authentication mode used by AP
				//, get_mac_string(EVENT_HANDLER_ARG_FIELD(wifi_event_ap_probe_req_rx_t, mac)));

				ESP_LOGD(TAG,   "WIFI_EVENT_STA_CONNECTED. ");
				wifi_event_sta_connected_t * s =(wifi_event_sta_connected_t*)event_data;
				char * bssid = get_mac_string(s->bssid);
				char * ssid = strdup((char*)s->ssid);
				ESP_LOGD(TAG,   "WIFI_EVENT_STA_CONNECTED. Channel: %d, Access point: %s, BSSID: %s ", s->channel, STR_OR_BLANK(ssid), (bssid));
				FREE_AND_NULL(bssid);
				FREE_AND_NULL(ssid);

			}
				break;

			case WIFI_EVENT_STA_DISCONNECTED:{
//		    		structwifi_event_sta_disconnected_t
//		    		Argument structure for WIFI_EVENT_STA_DISCONNECTED event
//
//		    		Public Members
//
//		    		uint8_t ssid[32]
//		    		SSID of disconnected AP
//
//		    		uint8_t ssid_len
//		    		SSID length of disconnected AP
//
//		    		uint8_t bssid[6]
//		    		BSSID of disconnected AP
//
//		    		uint8_t reason
//		    		reason of disconnection
				wifi_event_sta_disconnected_t * s =(wifi_event_sta_disconnected_t*)event_data;
				char * bssid = get_mac_string(s->bssid);
				ESP_LOGD(TAG,   "WIFI_EVENT_STA_DISCONNECTED. From BSSID: %s, reason code: %d (%s)", STR_OR_BLANK(bssid),s->reason, get_disconnect_code_desc(s->reason));
				FREE_AND_NULL(bssid);
				if(last_connected>0) total_connected_time+=((esp_timer_get_time()-last_connected)/(1000*1000));
				last_connected = 0;
				num_disconnect++;
				ESP_LOGW(TAG,  "Wifi disconnected. Number of disconnects: %d, Average time connected: %d", num_disconnect, num_disconnect>0?(total_connected_time/num_disconnect):0);

				/* if a DISCONNECT message is posted while a scan is in progress this scan will NEVER end, causing scan to never work again. For this reason SCAN_BIT is cleared too */
				xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_WIFI_CONNECTED_BIT | WIFI_MANAGER_SCAN_BIT);


				// We want to process this message asynchronously, so make sure we copy the event buffer
				ESP_LOGD(TAG,  "Preparing to trigger event EVENT_STA_DISCONNECTED ");
				void * parm=malloc(sizeof(wifi_event_sta_disconnected_t));
				memcpy(parm,event_data,sizeof(wifi_event_sta_disconnected_t));
				ESP_LOGD(TAG,  "Triggering EVENT_STA_DISCONNECTED ");
				/* post disconnect event with reason code */
				wifi_manager_send_message(EVENT_STA_DISCONNECTED, parm );
			}
				break;

			default:
				break;
		}
    }
    else if(event_base== IP_EVENT){
		switch (event_id) {
			case IP_EVENT_STA_GOT_IP:{
//		    		structip_event_got_ip_t
//			    tcpip_adapter_if_t if_index;        /*!< Interface for which the event is received */
//			    tcpip_adapter_ip6_info_t ip6_info;  /*!< IPv6 address of the interface */
//		    	//	Event structure for IP_EVENT_STA_GOT_IP, IP_EVENT_ETH_GOT_IP events
//
//		    		Public Members
//
//		    		tcpip_adapter_if_tif_index
//		    		Interface for which the event is received
//
//		    		tcpip_adapter_ip_info_t ip_info
//		    		IP address, netmask, gatway IP address
//
//		    		bool ip_changed
//		    		Whether the assigned IP has changed or not

				ip_event_got_ip_t * s =(ip_event_got_ip_t*)event_data;
				//tcpip_adapter_if_t index = s->if_index;
				const tcpip_adapter_ip_info_t *ip_info = &s->ip_info;
				ESP_LOGI(TAG,   "SYSTEM_EVENT_STA_GOT_IP. IP="IPSTR", Gateway="IPSTR", NetMask="IPSTR", %s",
						IP2STR(&ip_info->ip),
						IP2STR(&ip_info->gw),
						IP2STR(&ip_info->netmask),
								s->ip_changed?"Address was changed":"Address unchanged");
				// todo: if ip address was changed, we probably need to restart, as all sockets
				// will become abnormal
				xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_WIFI_CONNECTED_BIT);
				last_connected = esp_timer_get_time();

				void * parm=malloc(sizeof(ip_event_got_ip_t));
				memcpy(parm,event_data,sizeof(ip_event_got_ip_t));
				wifi_manager_send_message(EVENT_STA_GOT_IP, parm );
			}
				break;
			case IP_EVENT_STA_LOST_IP:
				ESP_LOGD(TAG,   "IP_EVENT_STA_LOST_IP");
				break;
			case IP_EVENT_AP_STAIPASSIGNED:
				ESP_LOGD(TAG,   "IP_EVENT_AP_STAIPASSIGNED");
				break;
			case IP_EVENT_GOT_IP6:
				ESP_LOGD(TAG,   "IP_EVENT_GOT_IP6");
				break;
			case IP_EVENT_ETH_GOT_IP:
				ESP_LOGD(TAG,   "IP_EVENT_ETH_GOT_IP");
				break;
			default:
				break;
		}
	}

}


wifi_config_t* wifi_manager_get_wifi_sta_config(){
	return wifi_manager_config_sta;
}



void wifi_manager_connect_async(){
	/* in order to avoid a false positive on the front end app we need to quickly flush the ip json
	 * There'se a risk the front end sees an IP or a password error when in fact
	 * it's a remnant from a previous connection
	 */
	if(wifi_manager_lock_json_buffer( portMAX_DELAY )){
		ip_info_cjson= wifi_manager_clear_ip_info_json(&ip_info_cjson);
		wifi_manager_unlock_json_buffer();
	}
	wifi_manager_send_message(ORDER_CONNECT_STA, (void*)CONNECTION_REQUEST_USER);
}


char* wifi_manager_alloc_get_ip_info_json(){
	return cJSON_PrintUnformatted(ip_info_cjson);
}

void wifi_manager_destroy(){
	vTaskDelete(task_wifi_manager);
	task_wifi_manager = NULL;
	/* heap buffers */
	free(ip_info_json);
	free(release_url);
	cJSON_Delete(ip_info_cjson);
	cJSON_Delete(accessp_cjson);
	ip_info_cjson=NULL;
	accessp_cjson=NULL;
	free(wifi_manager_sta_ip);
	wifi_manager_sta_ip = NULL;
	if(wifi_manager_config_sta){
		free(wifi_manager_config_sta);
		wifi_manager_config_sta = NULL;
	}

	/* RTOS objects */
	vSemaphoreDelete(wifi_manager_json_mutex);
	wifi_manager_json_mutex = NULL;
	vSemaphoreDelete(wifi_manager_sta_ip_mutex);
	wifi_manager_sta_ip_mutex = NULL;
	vEventGroupDelete(wifi_manager_event_group);
	wifi_manager_event_group = NULL;
	vQueueDelete(wifi_manager_queue);
	wifi_manager_queue = NULL;
}

void wifi_manager_filter_unique( wifi_ap_record_t * aplist, uint16_t * aps) {
	int total_unique;
	wifi_ap_record_t * first_free;
	total_unique=*aps;

	first_free=NULL;

	for(int i=0; i<*aps-1;i++) {
		wifi_ap_record_t * ap = &aplist[i];

		/* skip the previously removed APs */
		if (ap->ssid[0] == 0) continue;

		/* remove the identical SSID+authmodes */
		for(int j=i+1; j<*aps;j++) {
			wifi_ap_record_t * ap1 = &aplist[j];
			if ( (strcmp((const char *)ap->ssid, (const char *)ap1->ssid)==0) && 
			     (ap->authmode == ap1->authmode) ) { /* same SSID, different auth mode is skipped */
				/* save the rssi for the display */
				if ((ap1->rssi) > (ap->rssi)) ap->rssi=ap1->rssi;
				/* clearing the record */
				memset(ap1,0, sizeof(wifi_ap_record_t));
			}
		}
	}
	/* reorder the list so APs follow each other in the list */
	for(int i=0; i<*aps;i++) {
		wifi_ap_record_t * ap = &aplist[i];
		/* skipping all that has no name */
		if (ap->ssid[0] == 0) {
			/* mark the first free slot */
			if (first_free==NULL) first_free=ap;
			total_unique--;
			continue;
		}
		if (first_free!=NULL) {
			memcpy(first_free, ap, sizeof(wifi_ap_record_t));
			memset(ap,0, sizeof(wifi_ap_record_t));
			/* find the next free slot */
			for(int j=0; j<*aps;j++) {
				if (aplist[j].ssid[0]==0) {
					first_free=&aplist[j];
					break;
				}
			}
		}
	}
	/* update the length of the list */
	*aps = total_unique;
}

BaseType_t wifi_manager_send_message_to_front(message_code_t code, void *param){
	queue_message msg;
	msg.code = code;
	msg.param = param;
	return xQueueSendToFront( wifi_manager_queue, &msg, portMAX_DELAY);
}

BaseType_t wifi_manager_send_message(message_code_t code, void *param){
	queue_message msg;
	msg.code = code;
	msg.param = param;
	return xQueueSend( wifi_manager_queue, &msg, portMAX_DELAY);
}

void wifi_manager_set_callback(message_code_t message_code, void (*func_ptr)(void*) ){
	if(cb_ptr_arr && message_code < MESSAGE_CODE_COUNT){
		cb_ptr_arr[message_code] = func_ptr;
	}
}
void wifi_manager_register_handlers(){
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_WIFI_READY, &event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, &event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_AUTHMODE_CHANGE, &event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_START, &event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STOP, &event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_PROBEREQRECVED, &event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_WPS_ER_SUCCESS, &event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_WPS_ER_FAILED, &event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_WPS_ER_TIMEOUT, &event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_WPS_ER_PIN, &event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, &event_handler, NULL ));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, &event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, &event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_STOP, &event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_LOST_IP, &event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_GOT_IP6, &event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &event_handler, NULL));
}

void wifi_manager_config_ap(){
	/* SoftAP - Wifi Access Point configuration setup */
		tcpip_adapter_ip_info_t info;
		esp_err_t err=ESP_OK;
		memset(&info, 0x00, sizeof(info));
		char * value = NULL;
		wifi_config_t ap_config = {
			.ap = {
				.ssid_len = 0,
			},
		};
		ESP_LOGI(TAG,  "Configuring Access Point.");

		ESP_LOGD(TAG,"Stopping DHCP on interface ");
		if((err= tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP))!=ESP_OK) 	/* stop AP DHCP server */
		{
			ESP_LOGW(TAG,  "Stopping DHCP failed. Error %s",esp_err_to_name(err));
		}
		/*
		 * Set access point mode IP adapter configuration
		 */
		value = config_alloc_get_default(NVS_TYPE_STR, "ap_ip_address", DEFAULT_AP_IP, 0);
		if(value!=NULL){
			ESP_LOGD(TAG,  "IP Address: %s", value);
			inet_pton(AF_INET,value, &info.ip); /* access point is on a static IP */
		}
		FREE_AND_NULL(value);
		value = config_alloc_get_default(NVS_TYPE_STR, "ap_ip_gateway", CONFIG_DEFAULT_AP_GATEWAY, 0);
		if(value!=NULL){
			ESP_LOGD(TAG,  "Gateway: %s", value);
			inet_pton(AF_INET,value, &info.gw); /* access point is on a static IP */
		}
		FREE_AND_NULL(value);
		value = config_alloc_get_default(NVS_TYPE_STR, "ap_ip_netmask", CONFIG_DEFAULT_AP_NETMASK, 0);
		if(value!=NULL){
			ESP_LOGD(TAG,  "Netmask: %s", value);
			inet_pton(AF_INET,value, &info.netmask); /* access point is on a static IP */
		}
		FREE_AND_NULL(value);

		ESP_LOGD(TAG,  "Setting tcp_ip info for interface TCPIP_ADAPTER_IF_AP");
		if((err=tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &info))!=ESP_OK){
			ESP_LOGE(TAG,  "Setting tcp_ip info for interface TCPIP_ADAPTER_IF_AP. Error %s",esp_err_to_name(err));
			return;
		}
		/*
		 * Set Access Point configuration
		 */
		value = config_alloc_get_default(NVS_TYPE_STR, "ap_ssid", CONFIG_DEFAULT_AP_SSID, 0);
		if(value!=NULL){
			strlcpy((char *)ap_config.ap.ssid, value,sizeof(ap_config.ap.ssid) );
			ESP_LOGI(TAG,  "AP SSID: %s", (char *)ap_config.ap.ssid);
		}
		FREE_AND_NULL(value);

		value = config_alloc_get_default(NVS_TYPE_STR, "ap_pwd", DEFAULT_AP_PASSWORD, 0);
		if(value!=NULL){
			strlcpy((char *)ap_config.ap.password, value,sizeof(ap_config.ap.password) );
			ESP_LOGI(TAG,  "AP Password: %s", (char *)ap_config.ap.password);
		}
		FREE_AND_NULL(value);

		value = config_alloc_get_default(NVS_TYPE_STR, "ap_channel", STR(CONFIG_DEFAULT_AP_CHANNEL), 0);
		if(value!=NULL){
			ESP_LOGD(TAG,  "Channel: %s", value);
			ap_config.ap.channel=atoi(value);
		}
		FREE_AND_NULL(value);

		ap_config.ap.authmode = AP_AUTHMODE;
		ap_config.ap.ssid_hidden = DEFAULT_AP_SSID_HIDDEN;
		ap_config.ap.max_connection = DEFAULT_AP_MAX_CONNECTIONS;
		ap_config.ap.beacon_interval = DEFAULT_AP_BEACON_INTERVAL;

		ESP_LOGD(TAG,  "Auth Mode: %d", ap_config.ap.authmode);
		ESP_LOGD(TAG,  "SSID Hidden: %d", ap_config.ap.ssid_hidden);
		ESP_LOGD(TAG,  "Max Connections: %d", ap_config.ap.max_connection);
		ESP_LOGD(TAG,  "Beacon interval: %d", ap_config.ap.beacon_interval);

		ESP_LOGD(TAG,  "");
		if((err= esp_wifi_set_mode(WIFI_MODE_APSTA))!=ESP_OK) 	/* stop AP DHCP server */
		{
			ESP_LOGE(TAG,  "Setting wifi mode as WIFI_MODE_APSTA failed. Error %s",esp_err_to_name(err));
			return;
		}



		ESP_LOGD(TAG,  "Setting wifi AP configuration for WIFI_IF_AP");
		if((err= esp_wifi_set_config(WIFI_IF_AP, &ap_config))!=ESP_OK) 	/* stop AP DHCP server */
		{
			ESP_LOGE(TAG,  "Setting wifi AP configuration for WIFI_IF_AP failed. Error %s",esp_err_to_name(err));
			return;
		}


		ESP_LOGD(TAG,  "Setting wifi bandwidth (%d) for WIFI_IF_AP",DEFAULT_AP_BANDWIDTH);
		if((err=esp_wifi_set_bandwidth(WIFI_IF_AP, DEFAULT_AP_BANDWIDTH))!=ESP_OK) 	/* stop AP DHCP server */
		{
			ESP_LOGE(TAG,  "Setting wifi bandwidth for WIFI_IF_AP failed. Error %s",esp_err_to_name(err));
			return;
		}

		ESP_LOGD(TAG,  "Setting wifi power save (%d) for WIFI_IF_AP",DEFAULT_STA_POWER_SAVE);

		if((err=esp_wifi_set_ps(DEFAULT_STA_POWER_SAVE))!=ESP_OK) 	/* stop AP DHCP server */
		{
			ESP_LOGE(TAG,  "Setting wifi power savefor WIFI_IF_AP failed. Error %s",esp_err_to_name(err));
			return;
		}

		ESP_LOGD(TAG,  "Starting dhcps on interface TCPIP_ADAPTER_IF_AP");

		if((err=tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP))!=ESP_OK) 	/* stop AP DHCP server */
		{
			ESP_LOGE(TAG, "Starting dhcp on TCPIP_ADAPTER_IF_AP failed. Error %s",esp_err_to_name(err));
			return;
		}

		ESP_LOGD(TAG,  "Done configuring Soft Access Point");
		dns_server_start();


}

void wifi_manager( void * pvParameters ){
	queue_message msg;
	BaseType_t xStatus;
	EventBits_t uxBits;
	uint8_t	retries = 0;
	esp_err_t err=ESP_OK;
	TimerHandle_t STA_timer;
	uint32_t STA_duration = STA_POLLING_MIN;
	
	/* create timer for background STA connection */
	STA_timer = xTimerCreate("background STA", pdMS_TO_TICKS(STA_duration), pdFALSE, NULL, polling_STA);		

	/* start http server */
	http_server_start();

	/* enqueue first event: load previous config and start AP or STA mode */
	wifi_manager_send_message(ORDER_LOAD_AND_RESTORE_STA, NULL);
	/* main processing loop */
	for(;;){
		xStatus = xQueueReceive( wifi_manager_queue, &msg, portMAX_DELAY );

		if( xStatus == pdPASS ){
			switch(msg.code){

			case EVENT_SCAN_DONE:
				/* As input param, it stores max AP number ap_records can hold. As output param, it receives the actual AP number this API returns.
				 * As a consequence, ap_num MUST be reset to MAX_AP_NUM at every scan */
				ESP_LOGD(TAG,  "Getting AP list records");
				if((err=esp_wifi_scan_get_ap_num(&ap_num))!=ESP_OK) {
					ESP_LOGE(TAG,  "Failed to retrieve scan results count. Error %s",esp_err_to_name(err));
					break;
				}

				if(ap_num>0){
					accessp_records = (wifi_ap_record_t*)malloc(sizeof(wifi_ap_record_t) * ap_num);
					if((err=esp_wifi_scan_get_ap_records(&ap_num, accessp_records))!=ESP_OK) {
						ESP_LOGE(TAG,  "Failed to retrieve scan results list. Error %s",esp_err_to_name(err));
						break;
					}
					/* make sure the http server isn't trying to access the list while it gets refreshed */
					ESP_LOGD(TAG,  "Preparing to build ap JSON list");
					if(wifi_manager_lock_json_buffer( pdMS_TO_TICKS(1000) )){
						/* Will remove the duplicate SSIDs from the list and update ap_num */
						wifi_manager_filter_unique(accessp_records, &ap_num);
						wifi_manager_generate_access_points_json(&accessp_cjson);
						wifi_manager_unlock_json_buffer();
						ESP_LOGD(TAG,  "Done building ap JSON list");

					}
					else{
						ESP_LOGE(TAG,   "could not get access to json mutex in wifi_scan");
					}
					free(accessp_records);
				}
				else{
					//
					ESP_LOGD(TAG,  "No AP Found.  Emptying the list.");
					accessp_cjson = wifi_manager_get_new_array_json(&accessp_cjson);
				}

				/* callback */
				if(cb_ptr_arr[msg.code]) {
					ESP_LOGD(TAG,  "Invoking SCAN DONE callback");
					(*cb_ptr_arr[msg.code])(NULL);
					ESP_LOGD(TAG,  "Done Invoking SCAN DONE callback");
				}
				break;

			case ORDER_START_WIFI_SCAN:
				ESP_LOGD(TAG,   "MESSAGE: ORDER_START_WIFI_SCAN");

				/* if a scan is already in progress this message is simply ignored thanks to the WIFI_MANAGER_SCAN_BIT uxBit */
				if(! isGroupBitSet(WIFI_MANAGER_SCAN_BIT) ){
					if(esp_wifi_scan_start(&scan_config, false)!=ESP_OK){
						ESP_LOGW(TAG,  "Unable to start scan; wifi is trying to connect");
//						set_status_message(WARNING, "Wifi Connecting. Cannot start scan.");
						messaging_post_message(MESSAGING_WARNING,MESSAGING_CLASS_SYSTEM,"Wifi connecting. Cannot start scan.");
					}
					else {
						xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_SCAN_BIT);
					}
				}
				else {
					ESP_LOGW(TAG,  "Scan already in progress!");
				}


				/* callback */
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])(NULL);

				break;

			case ORDER_LOAD_AND_RESTORE_STA:
				ESP_LOGD(TAG,   "MESSAGE: ORDER_LOAD_AND_RESTORE_STA. About to fetch wifi STA configuration");
				if(wifi_manager_fetch_wifi_sta_config()){
					ESP_LOGI(TAG,   "Saved wifi found on startup. Will attempt to connect.");
					wifi_manager_send_message(ORDER_CONNECT_STA, (void*)CONNECTION_REQUEST_RESTORE_CONNECTION);
				}
				else{
					/* no wifi saved: start soft AP! This is what should happen during a first run */
					ESP_LOGD(TAG,   "No saved wifi found on startup. Starting access point.");
					wifi_manager_send_message(ORDER_START_AP, NULL);
				}

				/* callback */
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])(NULL);

				break;

			case ORDER_CONNECT_STA:
				ESP_LOGD(TAG,   "MESSAGE: ORDER_CONNECT_STA - Begin");

				/* very important: precise that this connection attempt is specifically requested.
				 * Param in that case is a boolean indicating if the request was made automatically
				 * by the wifi_manager.
				 * */
				if((BaseType_t)msg.param == CONNECTION_REQUEST_USER) {
					ESP_LOGD(TAG,   "MESSAGE: ORDER_CONNECT_STA - Connection request with no nvs connection saved yet");
					xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_STA_CONNECT_BIT);
					xEventGroupClearBits(wifi_manager_event_group,WIFI_MANAGER_REQUEST_STA_CONNECT_FAILED_BIT);
				}
				else if((BaseType_t)msg.param == CONNECTION_REQUEST_RESTORE_CONNECTION) {
					ESP_LOGD(TAG,   "MESSAGE: ORDER_CONNECT_STA - Connection request after restoring the AP configuration");
					xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_RESTORE_STA_BIT);

					/* STA - Wifi Station configuration setup */
					//todo:  support static ip address
//					if(wifi_settings.sta_static_ip) {
//						// There's a static ip address configured, so
//						ESP_LOGD(TAG,   "Assigning static ip to STA interface. IP: %s , GW: %s , Mask: %s",
//										ip4addr_ntoa(&wifi_settings.sta_static_ip_config.ip),
//										ip4addr_ntoa(&wifi_settings.sta_static_ip_config.gw),
//										ip4addr_ntoa(&wifi_settings.sta_static_ip_config.netmask));
//
//						/* stop DHCP client*/
//						ESP_ERROR_CHECK(tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA));
//						/* assign a static IP to the STA network interface */
//						ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &wifi_settings.sta_static_ip_config));
//						}
//					else {
						/* start DHCP client if not started*/
						tcpip_adapter_dhcp_status_t status;
						ESP_LOGD(TAG,   "wifi_manager: Checking if DHCP client for STA interface is running");
						ESP_ERROR_CHECK_WITHOUT_ABORT(tcpip_adapter_dhcpc_get_status(TCPIP_ADAPTER_IF_STA, &status));
						if (status!=TCPIP_ADAPTER_DHCP_STARTED) {
							ESP_LOGD(TAG,   "wifi_manager: Start DHCP client for STA interface");
							ESP_ERROR_CHECK_WITHOUT_ABORT(tcpip_adapter_dhcpc_start(TCPIP_ADAPTER_IF_STA));
						}
					//}
				}

				uxBits = xEventGroupGetBits(wifi_manager_event_group);
				if( uxBits & WIFI_MANAGER_WIFI_CONNECTED_BIT ){
					ESP_LOGD(TAG,   "MESSAGE: ORDER_CONNECT_STA - Wifi connected bit set, ordering disconnect (WIFI_MANAGER_WIFI_CONNECTED_BIT)");
					wifi_manager_send_message(ORDER_DISCONNECT_STA, NULL);
					/* todo: reconnect */
				}
				else{
					wifi_mode_t mode;
					/* update config to latest and attempt connection */
					esp_wifi_get_mode(&mode);
					if( WIFI_MODE_APSTA != mode && WIFI_MODE_STA !=mode ){
						// the soft ap is not started, so let's set the WiFi mode to STA
						ESP_LOGD(TAG,   "MESSAGE: ORDER_CONNECT_STA - setting mode WIFI_MODE_STA");
						if((err=esp_wifi_set_mode(WIFI_MODE_STA))!=ESP_OK) {
							ESP_LOGE(TAG,  "Failed to set wifi mode to STA. Error %s",esp_err_to_name(err));
							break;
						}
					}
					ESP_LOGD(TAG,   "MESSAGE: ORDER_CONNECT_STA - setting config for WIFI_IF_STA");
					wifi_config_t* cfg = wifi_manager_get_wifi_sta_config();
				    char * scan_mode = config_alloc_get_default(NVS_TYPE_STR, "wifi_smode", "f", 0);
				    if (scan_mode && strcasecmp(scan_mode,"a")==0) {
				    	cfg->sta.scan_method=WIFI_ALL_CHANNEL_SCAN;
				    }
				    else {
				    	cfg->sta.scan_method=WIFI_FAST_SCAN;
				    }
				    FREE_AND_NULL(scan_mode);
					if((err=esp_wifi_set_config(WIFI_IF_STA, cfg))!=ESP_OK) {
						ESP_LOGE(TAG,  "Failed to set STA configuration. Error %s",esp_err_to_name(err));
						break;
					}

					set_host_name();
					ESP_LOGI(TAG,  "Wifi Connecting...");
					if((err=esp_wifi_connect())!=ESP_OK) {
						ESP_LOGE(TAG,  "Failed to initiate wifi connection. Error %s",esp_err_to_name(err));
						break;
					}
				}

				/* callback */
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])(NULL);

				break;

			case EVENT_STA_DISCONNECTED:{
				wifi_event_sta_disconnected_t disc_event;

				ESP_LOGD(TAG,   "MESSAGE: EVENT_STA_DISCONNECTED");
				if(msg.param == NULL){
					ESP_LOGE(TAG,  "MESSAGE: EVENT_STA_DISCONNECTED - expected parameter not found!");
				}
				else{
					memcpy(&disc_event,(wifi_event_sta_disconnected_t*)msg.param,sizeof(disc_event));
					free(msg.param);
					ESP_LOGD(TAG,   "MESSAGE: EVENT_STA_DISCONNECTED with Reason code: %d (%s)", disc_event.reason, get_disconnect_code_desc(disc_event.reason));
				}

				/* this even can be posted in numerous different conditions
				 *
				 * 1. SSID password is wrong
				 * 2. Manual disconnection ordered
				 * 3. Connection lost
				 *
				 * Having clear understand as to WHY the event was posted is key to having an efficient wifi manager
				 *
				 * With wifi_manager, we determine:
				 *  If WIFI_MANAGER_REQUEST_STA_CONNECT_BIT is set, We consider it's a client that requested the connection.
				 *    When SYSTEM_EVENT_STA_DISCONNECTED is posted, it's probably a password/something went wrong with the handshake.
				 *
				 *  If WIFI_MANAGER_REQUEST_STA_CONNECT_BIT is set, it's a disconnection that was ASKED by the client (clicking disconnect in the app)
				 *    When SYSTEM_EVENT_STA_DISCONNECTED is posted, saved wifi is erased from the NVS memory.
				 *
				 *  If WIFI_MANAGER_REQUEST_STA_CONNECT_BIT and WIFI_MANAGER_REQUEST_STA_CONNECT_BIT are NOT set, it's a lost connection
				 *
				 *  In this version of the software, reason codes are not used. They are indicated here for potential future usage.
				 *
				 *  REASON CODE:
				 *  1		UNSPECIFIED
				 *  2		AUTH_EXPIRE					auth no longer valid, this smells like someone changed a password on the AP
				 *  3		AUTH_LEAVE
				 *  4		ASSOC_EXPIRE
				 *  5		ASSOC_TOOMANY				too many devices already connected to the AP => AP fails to respond
				 *  6		NOT_AUTHED
				 *  7		NOT_ASSOCED
				 *  8		ASSOC_LEAVE
				 *  9		ASSOC_NOT_AUTHED
				 *  10		DISASSOC_PWRCAP_BAD
				 *  11		DISASSOC_SUPCHAN_BAD
				 *	12		<n/a>
				 *  13		IE_INVALID
				 *  14		MIC_FAILURE
				 *  15		4WAY_HANDSHAKE_TIMEOUT		wrong password! This was personnaly tested on my home wifi with a wrong password.
				 *  16		GROUP_KEY_UPDATE_TIMEOUT
				 *  17		IE_IN_4WAY_DIFFERS
				 *  18		GROUP_CIPHER_INVALID
				 *  19		PAIRWISE_CIPHER_INVALID
				 *  20		AKMP_INVALID
				 *  21		UNSUPP_RSN_IE_VERSION
				 *  22		INVALID_RSN_IE_CAP
				 *  23		802_1X_AUTH_FAILED			wrong password?
				 *  24		CIPHER_SUITE_REJECTED
				 *  200		BEACON_TIMEOUT
				 *  201		NO_AP_FOUND
				 *  202		AUTH_FAIL
				 *  203		ASSOC_FAIL
				 *  204		HANDSHAKE_TIMEOUT
				 *
				 * */

				/* reset saved sta IP */
				wifi_manager_safe_update_sta_ip_string((struct ip4_addr * )0);

				uxBits = xEventGroupGetBits(wifi_manager_event_group);
				if( uxBits & WIFI_MANAGER_REQUEST_STA_CONNECT_BIT ){
					xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_STA_CONNECT_BIT);
					ESP_LOGW(TAG,   "WiFi Disconnected while processing user connect request.  Wrong password?");
					/* there are no retries when it's a user requested connection by design. This avoids a user hanging too much
					 * in case they typed a wrong password for instance. Here we simply clear the request bit and move on */
					
					if(wifi_manager_lock_json_buffer( portMAX_DELAY )){
						wifi_manager_generate_ip_info_json( UPDATE_FAILED_ATTEMPT );
						wifi_manager_unlock_json_buffer();
					}
					wifi_mode_t mode;
					esp_wifi_get_mode(&mode);
					if( WIFI_MODE_STA ==mode ){
						xEventGroupSetBits(wifi_manager_event_group,WIFI_MANAGER_REQUEST_STA_CONNECT_FAILED_BIT);
						// if wifi was STA, attempt to reload the previous network connection
						ESP_LOGW(TAG,"Attempting to restore previous network"); 
						wifi_manager_send_message(ORDER_LOAD_AND_RESTORE_STA, NULL);
					}
				}
				else if (uxBits & WIFI_MANAGER_REQUEST_DISCONNECT_BIT){
					ESP_LOGD(TAG,   "WiFi disconnected by user");
					/* user manually requested a disconnect so the lost connection is a normal event. Clear the flag and restart the AP */
					xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_DISCONNECT_BIT);
					if(wifi_manager_lock_json_buffer( portMAX_DELAY )){
						wifi_manager_generate_ip_info_json( UPDATE_USER_DISCONNECT );
						wifi_manager_unlock_json_buffer();
					}
					/* erase configuration */
					if(wifi_manager_config_sta){
						ESP_LOGI(TAG,   "Erasing WiFi Configuration.");
						memset(wifi_manager_config_sta, 0x00, sizeof(wifi_config_t));
						/* save NVS memory */
						wifi_manager_save_sta_config();
					}
					/* start SoftAP */
					ESP_LOGD(TAG,   "Disconnect processing complete. Ordering an AP start.");
					wifi_manager_send_message(ORDER_START_AP, NULL);
				}
				else{
					/* lost connection ? */
					ESP_LOGE(TAG,   "WiFi Connection lost.");
					messaging_post_message(MESSAGING_WARNING,MESSAGING_CLASS_SYSTEM,"WiFi Connection lost");

					if(wifi_manager_lock_json_buffer( portMAX_DELAY )){
						wifi_manager_generate_ip_info_json( UPDATE_LOST_CONNECTION );
						wifi_manager_unlock_json_buffer();
					}

					if(retries < WIFI_MANAGER_MAX_RETRY){
						ESP_LOGD(TAG,   "Issuing ORDER_CONNECT_STA to retry connection.");
						retries++;
						wifi_manager_send_message(ORDER_CONNECT_STA, (void*)CONNECTION_REQUEST_AUTO_RECONNECT);
					}
					else{
						/* In this scenario the connection was lost beyond repair: kick start the AP! */
						retries = 0;
						wifi_mode_t mode;
						ESP_LOGW(TAG,   "All connect retry attempts failed.");
						
						/* put us in softAP mode first */
						esp_wifi_get_mode(&mode);
						/* if it was a restore attempt connection, we clear the bit */
						xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_RESTORE_STA_BIT);
						
						if(WIFI_MODE_APSTA != mode){
							/* call directly config_ap because we don't want to scan so the message has no benefit */
							ESP_LOGD(TAG,   "Starting AP directly.");
							wifi_manager_config_ap();		
							STA_duration = STA_POLLING_MIN;							
							/* manual callback if needed */
							if(cb_ptr_arr[ORDER_START_AP]) (*cb_ptr_arr[ORDER_START_AP])(NULL);
						}
						else if(STA_duration < STA_POLLING_MAX) {
							STA_duration *= 1.25;
						}	
						
						xTimerChangePeriod(STA_timer, pdMS_TO_TICKS(STA_duration), portMAX_DELAY);
						xTimerStart(STA_timer, portMAX_DELAY);						
						ESP_LOGD(TAG,   "STA search slow polling of %d", STA_duration);
					}
				}
								
				/* callback */
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])(NULL);
			}
				break;

			case ORDER_START_AP:
				ESP_LOGD(TAG,   "MESSAGE: ORDER_START_AP");
				wifi_manager_config_ap();
				ESP_LOGD(TAG,  "AP Starting, requesting wifi scan.");
				wifi_manager_scan_async();
				/* callback */
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])(NULL);
				break;

			case EVENT_STA_GOT_IP:
				ESP_LOGD(TAG,   "MESSAGE: EVENT_STA_GOT_IP");

				uxBits = xEventGroupGetBits(wifi_manager_event_group);

				/* reset connection requests bits -- doesn't matter if it was set or not */
				xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_STA_CONNECT_BIT);

				/* save IP as a string for the HTTP server host */
				//s->ip_info.ip.addr
				ip_event_got_ip_t * event =(ip_event_got_ip_t*)msg.param;
				wifi_manager_safe_update_sta_ip_string(&(event->ip_info.ip));
				free(msg.param);

				/* save wifi config in NVS if it wasn't a restored of a connection */
				if(uxBits & WIFI_MANAGER_REQUEST_RESTORE_STA_BIT){
					ESP_LOGD(TAG,  "Configuration came from nvs, no need to save.");
					xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_RESTORE_STA_BIT);
				}
				else{
					ESP_LOGD(TAG,  "Connection was initiated by user, storing config to nvs.");
					wifi_manager_save_sta_config();
				}

				/* refresh JSON with the new IP */
				if(wifi_manager_lock_json_buffer( portMAX_DELAY )){
					/* generate the connection info with success */
					wifi_manager_generate_ip_info_json( uxBits & WIFI_MANAGER_REQUEST_STA_CONNECT_FAILED_BIT?UPDATE_FAILED_ATTEMPT_AND_RESTORE:UPDATE_CONNECTION_OK );
					wifi_manager_unlock_json_buffer();
				}
				else {
					ESP_LOGW(TAG,  "Unable to lock status json buffer. ");
				}

				/* bring down DNS hijack */
				ESP_LOGD(TAG,  "Stopping dns server.");
				dns_server_stop();
				
				/* stop AP mode */
				esp_wifi_set_mode(WIFI_MODE_STA);

				/* callback */
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])(NULL);
				break;
			case UPDATE_CONNECTION_OK:
				/* refresh JSON */
				if(wifi_manager_lock_json_buffer( portMAX_DELAY )){
					/* generate the connection info with success */
					wifi_manager_generate_ip_info_json( UPDATE_CONNECTION_OK );
					wifi_manager_unlock_json_buffer();
				}
				break;
			case ORDER_DISCONNECT_STA:
				ESP_LOGD(TAG,   "MESSAGE: ORDER_DISCONNECT_STA. Calling esp_wifi_disconnect()");

				/* precise this is coming from a user request */
				xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_DISCONNECT_BIT);

				/* order wifi discconect */
				ESP_ERROR_CHECK(esp_wifi_disconnect());

				/* callback */
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])(NULL);

				break;
			case  ORDER_RESTART_OTA:
				ESP_LOGD(TAG,   "Calling guided_restart_ota.");
				guided_restart_ota();
				break;
			case  ORDER_RESTART_OTA_URL:
				ESP_LOGD(TAG,   "Calling start_ota.");
				start_ota(msg.param, NULL, 0);
				free(msg.param);
				break;

			case  ORDER_RESTART_RECOVERY:
				ESP_LOGD(TAG,   "Calling guided_factory.");
				guided_factory();
				break;
			case	ORDER_RESTART:
				ESP_LOGD(TAG,   "Calling simple_restart.");
				simple_restart();
				break;
			case ORDER_UPDATE_STATUS:
				wifi_manager_update_basic_info();
				break;
			default:
				break;

			} /* end of switch/case */
		} /* end of if status=pdPASS */
	} /* end of for loop */

	vTaskDelete( NULL );
}
