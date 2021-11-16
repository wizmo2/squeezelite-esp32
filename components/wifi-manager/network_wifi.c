#include "network_wifi.h"
#include <string.h>
#include "cJSON.h"
#include "dns_server.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "globdefs.h"
#include "lwip/sockets.h"
#include "messaging.h"
#include "network_status.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "platform_config.h"
#include "platform_esp32.h"
#include "tools.h"
#include "trace.h"

static void network_wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void network_manager_wifi_ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) ;
static char* get_disconnect_code_desc(uint8_t reason);

cJSON* accessp_cjson = NULL;
wifi_config_t* wifi_manager_config_sta = NULL;
static const char TAG[] = "network_wifi";
const char wifi_manager_nvs_namespace[] = "config";

uint16_t ap_num = MAX_AP_NUM;




esp_netif_t *wifi_netif;

wifi_ap_record_t* accessp_records = NULL;
/* wifi scanner config */
wifi_scan_config_t scan_config = {
    .ssid = 0,
    .bssid = 0,
    .channel = 0,
    .show_hidden = true};
#ifndef STR_OR_BLANK
#define STR_OR_BLANK(p) p == NULL ? "" : p
#endif

esp_netif_t *network_wifi_get_interface(){
    return wifi_netif;
}

void init_network_wifi() {
    ESP_LOGD(TAG, "WIFI Starting.");
    accessp_cjson = NULL;
    accessp_cjson = wifi_manager_clear_ap_list_json(&accessp_cjson);
    wifi_manager_config_sta = (wifi_config_t*)malloc(sizeof(wifi_config_t));
    memset(wifi_manager_config_sta, 0x00, sizeof(wifi_config_t));
    ESP_LOGD(TAG, "Init. ");
    wifi_netif = esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_LOGD(TAG, "Handlers");
    //wifi_manager_register_handlers();
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &network_wifi_event_handler,
                                                        NULL,
                                                        NULL));
    esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &network_manager_wifi_ip_event_handler, NULL);
    ESP_LOGD(TAG, "Storage");
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_LOGD(TAG, "Set Mode");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    ESP_LOGD(TAG, "Start");
    ESP_ERROR_CHECK(esp_wifi_start());
}
void destroy_network_wifi() {
    cJSON_Delete(accessp_cjson);
    accessp_cjson = NULL;
    FREE_AND_NULL(wifi_manager_config_sta);
}
bool network_wifi_sta_config_changed()  {
    bool changed = true;
    wifi_config_t wifi_manager_config_sta_existing;
    if(wifi_manager_config_sta && wifi_manager_load_wifi_sta_config(&wifi_manager_config_sta_existing )){
        changed = strcmp( (char *)wifi_manager_config_sta_existing.sta.ssid,(char *)wifi_manager_config_sta->sta.ssid ) !=0 ||
                    strcmp((char *) wifi_manager_config_sta_existing.sta.password,(char *)wifi_manager_config_sta->sta.password ) !=0;
    }
    return changed;
    
}

esp_err_t network_wifi_save_sta_config() {
    nvs_handle handle;
    esp_err_t esp_err;
    ESP_LOGD(TAG, "Config Save");

    if (wifi_manager_config_sta) {
        esp_err = nvs_open(wifi_manager_nvs_namespace, NVS_READWRITE, &handle);
        if (esp_err != ESP_OK) {
            ESP_LOGE(TAG, "%s failure. Error %s", wifi_manager_nvs_namespace, esp_err_to_name(esp_err));
            return esp_err;
        }

        esp_err = nvs_set_blob(handle, "ssid", wifi_manager_config_sta->sta.ssid, sizeof(wifi_manager_config_sta->sta.ssid));
        if (esp_err != ESP_OK) {
            ESP_LOGE(TAG, "ssid (%s). Error %s", wifi_manager_nvs_namespace, esp_err_to_name(esp_err));
            return esp_err;
        }

        esp_err = nvs_set_blob(handle, "password", wifi_manager_config_sta->sta.password, sizeof(wifi_manager_config_sta->sta.password));
        if (esp_err != ESP_OK) {
            ESP_LOGE(TAG, "pass (%s). Error %s", wifi_manager_nvs_namespace, esp_err_to_name(esp_err));
            return esp_err;
        }

        esp_err = nvs_commit(handle);
        if (esp_err != ESP_OK) {
            ESP_LOGE(TAG, "Commit error: %s", esp_err_to_name(esp_err));
            messaging_post_message(MESSAGING_ERROR, MESSAGING_CLASS_SYSTEM, "Unable to save wifi credentials. %s", esp_err_to_name(esp_err));
            return esp_err;
        }
        nvs_close(handle);

        ESP_LOGD(TAG, "saved: ssid:%s password:%s", wifi_manager_config_sta->sta.ssid, wifi_manager_config_sta->sta.password);
    }

    return ESP_OK;
}
bool wifi_manager_load_wifi_sta_config(wifi_config_t* config ){
    nvs_handle handle;
    esp_err_t esp_err;

    ESP_LOGD(TAG, "Fetching wifi sta config.");
    esp_err = nvs_open(wifi_manager_nvs_namespace, NVS_READONLY, &handle);
    if (esp_err == ESP_OK) {


        /* ssid */
        ESP_LOGD(TAG, "Fetching value for ssid.");
        size_t sz = sizeof(config->sta.ssid);
        uint8_t* buff = (uint8_t*)malloc(sizeof(uint8_t) * sz);
        memset(buff, 0x00, sizeof(uint8_t) * sz);
        esp_err = nvs_get_blob(handle, "ssid", buff, &sz);
        if (esp_err != ESP_OK) {
            ESP_LOGD(TAG, "No ssid found in nvs.");
            FREE_AND_NULL(buff);
            nvs_close(handle);
            return false;
        }
        memcpy(config->sta.ssid, buff, sizeof(config->sta.ssid));
        FREE_AND_NULL(buff);
        ESP_LOGD(TAG, "wifi_manager_fetch_wifi_sta_config: ssid:%s ", config->sta.ssid);

        /* password */
        sz = sizeof(config->sta.password);
        buff = (uint8_t*)malloc(sizeof(uint8_t) * sz);
        memset(buff, 0x00, sizeof(uint8_t) * sz);
        esp_err = nvs_get_blob(handle, "password", buff, &sz);
        if (esp_err != ESP_OK) {
            // Don't take this as an error. This could be an opened access point?
            ESP_LOGW(TAG, "No wifi password found in nvs");
        } else {
            memcpy(config->sta.password, buff, sizeof(config->sta.password));
            ESP_LOGD(TAG, "wifi_manager_fetch_wifi_sta_config: password:%s", config->sta.password);
        }
        FREE_AND_NULL(buff);
        nvs_close(handle);
        config->sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
        return config->sta.ssid[0] != '\0';
    } else {
        ESP_LOGW(TAG, "wifi manager has no previous configuration. %s", esp_err_to_name(esp_err));
        return false;
    }
}

bool wifi_manager_fetch_wifi_sta_config() {
    if (wifi_manager_config_sta == NULL) {
        ESP_LOGD(TAG, "Allocating memory for structure.");
        wifi_manager_config_sta = (wifi_config_t*)malloc(sizeof(wifi_config_t));
    }
    memset(wifi_manager_config_sta, 0x00, sizeof(wifi_config_t));
    return wifi_manager_load_wifi_sta_config(wifi_manager_config_sta);
}

wifi_config_t* wifi_manager_get_wifi_sta_config() {
    return wifi_manager_config_sta;
}
// void wifi_manager_register_handlers() {
//     ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_WIFI_READY, &network_wifi_event_handler, NULL));
//     ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, &network_wifi_event_handler, NULL));
//     ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_AUTHMODE_CHANGE, &network_wifi_event_handler, NULL));
//     ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_START, &network_wifi_event_handler, NULL));
//     ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STOP, &network_wifi_event_handler, NULL));
//     ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_PROBEREQRECVED, &network_wifi_event_handler, NULL));
//     ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_WPS_ER_SUCCESS, &network_wifi_event_handler, NULL));
//     ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_WPS_ER_FAILED, &network_wifi_event_handler, NULL));
//     ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_WPS_ER_TIMEOUT, &network_wifi_event_handler, NULL));
//     ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_WPS_ER_PIN, &network_wifi_event_handler, NULL));
//     ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, &network_wifi_event_handler, NULL));
//     ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, &network_wifi_event_handler, NULL));
//     ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, &network_wifi_event_handler, NULL));
//     ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_STOP, &network_wifi_event_handler, NULL));
//     ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &network_wifi_event_handler, NULL));
//     ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &network_wifi_event_handler, NULL));
// }
#define LOCAL_MAC_SIZE 20
char* get_mac_string(uint8_t mac[6]) {
    char* macStr = malloc(LOCAL_MAC_SIZE);
    memset(macStr, 0x00, LOCAL_MAC_SIZE);
    snprintf(macStr, LOCAL_MAC_SIZE, MACSTR, MAC2STR(mac));
    return macStr;
}
static void network_wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base != WIFI_EVENT)
        return;
    switch (event_id) {
        case WIFI_EVENT_WIFI_READY:
            ESP_LOGD(TAG, "WIFI_EVENT_WIFI_READY");
            break;

        case WIFI_EVENT_SCAN_DONE:
            ESP_LOGD(TAG, "WIFI_EVENT_SCAN_DONE");
            network_manager_async_scan_done();
            break;

        case WIFI_EVENT_STA_AUTHMODE_CHANGE:
            ESP_LOGD(TAG, "WIFI_EVENT_STA_AUTHMODE_CHANGE");
            break;

        case WIFI_EVENT_AP_START:
            ESP_LOGD(TAG, "WIFI_EVENT_AP_START");
            break;

        case WIFI_EVENT_AP_STOP:
            ESP_LOGD(TAG, "WIFI_EVENT_AP_STOP");
            break;

        case WIFI_EVENT_AP_PROBEREQRECVED: {
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

            wifi_event_ap_probe_req_rx_t* s = (wifi_event_ap_probe_req_rx_t*)event_data;
            char* mac = get_mac_string(s->mac);
            ESP_LOGD(TAG, "WIFI_EVENT_AP_PROBEREQRECVED. RSSI: %d, MAC: %s", s->rssi, STR_OR_BLANK(mac));
            FREE_AND_NULL(mac);
        } break;
        case WIFI_EVENT_STA_WPS_ER_SUCCESS:
            ESP_LOGD(TAG, "WIFI_EVENT_STA_WPS_ER_SUCCESS");
            break;
        case WIFI_EVENT_STA_WPS_ER_FAILED:
            ESP_LOGD(TAG, "WIFI_EVENT_STA_WPS_ER_FAILED");
            break;
        case WIFI_EVENT_STA_WPS_ER_TIMEOUT:
            ESP_LOGD(TAG, "WIFI_EVENT_STA_WPS_ER_TIMEOUT");
            break;
        case WIFI_EVENT_STA_WPS_ER_PIN:
            ESP_LOGD(TAG, "WIFI_EVENT_STA_WPS_ER_PIN");
            break;
        case WIFI_EVENT_AP_STACONNECTED: {
            wifi_event_ap_staconnected_t* stac = (wifi_event_ap_staconnected_t*)event_data;
            char* mac = get_mac_string(stac->mac);
            ESP_LOGD(TAG, "WIFI_EVENT_AP_STACONNECTED. aid: %d, mac: %s", stac->aid, STR_OR_BLANK(mac));
            FREE_AND_NULL(mac);
        } break;
        case WIFI_EVENT_AP_STADISCONNECTED:
            ESP_LOGD(TAG, "WIFI_EVENT_AP_STADISCONNECTED");
            break;

        case WIFI_EVENT_STA_START:
            ESP_LOGD(TAG, "WIFI_EVENT_STA_START");
            break;

        case WIFI_EVENT_STA_STOP:
            ESP_LOGD(TAG, "WIFI_EVENT_STA_STOP");
            break;

        case WIFI_EVENT_STA_CONNECTED: {
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

            ESP_LOGD(TAG, "WIFI_EVENT_STA_CONNECTED. ");
            wifi_event_sta_connected_t* s = (wifi_event_sta_connected_t*)event_data;
            char* bssid = get_mac_string(s->bssid);
            char* ssid = strdup((char*)s->ssid);
            ESP_LOGD(TAG, "WIFI_EVENT_STA_CONNECTED. Channel: %d, Access point: %s, BSSID: %s ", s->channel, STR_OR_BLANK(ssid), (bssid));
            FREE_AND_NULL(bssid);
            FREE_AND_NULL(ssid);
            network_manager_async_success();

        } break;

        case WIFI_EVENT_STA_DISCONNECTED: {
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
            wifi_event_sta_disconnected_t* s = (wifi_event_sta_disconnected_t*)event_data;
            char* bssid = get_mac_string(s->bssid);
            ESP_LOGD(TAG, "WIFI_EVENT_STA_DISCONNECTED. From BSSID: %s, reason code: %d (%s)", STR_OR_BLANK(bssid), s->reason, get_disconnect_code_desc(s->reason));
            FREE_AND_NULL(bssid);

                
            /* if a DISCONNECT message is posted while a scan is in progress this scan will NEVER end, causing scan to never work again. For this reason SCAN_BIT is cleared too */
            // todo: check for implementation of this: network_manager_clear_flag(WIFI_MANAGER_WIFI_CONNECTED_BIT | WIFI_MANAGER_SCAN_BIT);
            wifi_event_sta_disconnected_t * disconnected_event = malloc(sizeof(wifi_event_sta_disconnected_t));
            memcpy(disconnected_event, event_data, sizeof(wifi_event_sta_disconnected_t));
            network_manager_async_lost_connection(disconnected_event);
        } break;

        default:
            break;
    }
}


cJSON* wifi_manager_get_new_array_json(cJSON** old) {
    ESP_LOGV(TAG, "wifi_manager_get_new_array_json called");
    cJSON* root = *old;
    if (root != NULL) {
        cJSON_Delete(root);
        *old = NULL;
    }
    ESP_LOGV(TAG, "wifi_manager_get_new_array_json done");
    return cJSON_CreateArray();
}
void wifi_manager_generate_access_points_json(cJSON** ap_list) {
    *ap_list = wifi_manager_get_new_array_json(ap_list);

    if (*ap_list == NULL)
        return;
    for (int i = 0; i < ap_num; i++) {
        cJSON* ap = cJSON_CreateObject();
        if (ap == NULL) {
            ESP_LOGE(TAG, "Unable to allocate memory for access point entry #%d", i);
            return;
        }
        cJSON* radio = cJSON_CreateObject();
        if (radio == NULL) {
            ESP_LOGE(TAG, "Unable to allocate memory for access point entry #%d", i);
            cJSON_Delete(ap);
            return;
        }
        wifi_ap_record_t ap_rec = accessp_records[i];
        cJSON_AddNumberToObject(ap, "chan", ap_rec.primary);
        cJSON_AddNumberToObject(ap, "rssi", ap_rec.rssi);
        cJSON_AddNumberToObject(ap, "auth", ap_rec.authmode);
        cJSON_AddItemToObject(ap, "ssid", cJSON_CreateString((char*)ap_rec.ssid));

        char* bssid = get_mac_string(ap_rec.bssid);
        cJSON_AddItemToObject(ap, "bssid", cJSON_CreateString(STR_OR_BLANK(bssid)));
        FREE_AND_NULL(bssid);
        cJSON_AddNumberToObject(radio, "b", ap_rec.phy_11b ? 1 : 0);
        cJSON_AddNumberToObject(radio, "g", ap_rec.phy_11g ? 1 : 0);
        cJSON_AddNumberToObject(radio, "n", ap_rec.phy_11n ? 1 : 0);
        cJSON_AddNumberToObject(radio, "low_rate", ap_rec.phy_lr ? 1 : 0);
        cJSON_AddItemToObject(ap, "radio", radio);
        cJSON_AddItemToArray(*ap_list, ap);
        char* ap_json = cJSON_PrintUnformatted(ap);
        if (ap_json != NULL) {
            ESP_LOGD(TAG, "New access point found: %s", ap_json);
            free(ap_json);
        }
    }
    char* ap_list_json = cJSON_PrintUnformatted(*ap_list);
    if (ap_list_json != NULL) {
        ESP_LOGV(TAG, "Full access point list: %s", ap_list_json);
        free(ap_list_json);
    }
}
void wifi_manager_set_ipv4val(const char* key, char* default_value, ip4_addr_t * target) {
    char* value = config_alloc_get_default(NVS_TYPE_STR, key, default_value, 0);
    if (value != NULL) {
        ESP_LOGD(TAG, "%s: %s", key, value);
        inet_pton(AF_INET, value, target); /* access point is on a static IP */
    }
    FREE_AND_NULL(value);
}
void wifi_manager_config_ap() {
    tcpip_adapter_ip_info_t info;
    esp_err_t err = ESP_OK;
    char* value = NULL;
    wifi_config_t ap_config = {
        .ap = {
            .ssid_len = 0,
        },
    };
    ESP_LOGI(TAG, "Configuring Access Point.");
    wifi_netif = esp_netif_create_default_wifi_ap();

    /* In order to change the IP info structure, we have to first stop 
     * the DHCP server on the new interface 
    */
    esp_netif_dhcps_stop(wifi_netif);

    // tcpip_adapter_dhcps_get_status(TCPIP_ADAPTER_IF_AP, &dhcp_status);
    // if (dhcp_status == TCPIP_ADAPTER_DHCP_STARTED) {
    //     ESP_LOGD(TAG, "Stopping DHCP on interface so we can ");
    //     if ((err = tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP)) != ESP_OK) /* stop AP DHCP server */
    //     {
    //         ESP_LOGW(TAG, "Stopping DHCP failed. Error %s", esp_err_to_name(err));
    //     }
    // }
    /*
    * Set access point mode IP adapter configuration
    */

    wifi_manager_set_ipv4val("ap_ip_address", DEFAULT_AP_IP, &info.ip);
    wifi_manager_set_ipv4val("ap_ip_gateway", CONFIG_DEFAULT_AP_GATEWAY, &info.gw);
    wifi_manager_set_ipv4val("ap_ip_netmask", CONFIG_DEFAULT_AP_NETMASK, &info.netmask);
    ESP_LOGD(TAG, "Setting tcp_ip info for interface TCPIP_ADAPTER_IF_AP");
    if ((err = esp_netif_set_ip_info(wifi_netif, &info)) != ESP_OK) {
        ESP_LOGE(TAG, "Setting tcp_ip info for interface TCPIP_ADAPTER_IF_AP. Error %s", esp_err_to_name(err));
        return;
    }
    /*
		 * Set Access Point configuration
		 */
    value = config_alloc_get_default(NVS_TYPE_STR, "ap_ssid", CONFIG_DEFAULT_AP_SSID, 0);
    if (value != NULL) {
        strlcpy((char*)ap_config.ap.ssid, value, sizeof(ap_config.ap.ssid));
        ESP_LOGI(TAG, "AP SSID: %s", (char*)ap_config.ap.ssid);
    }
    FREE_AND_NULL(value);

    value = config_alloc_get_default(NVS_TYPE_STR, "ap_pwd", DEFAULT_AP_PASSWORD, 0);
    if (value != NULL) {
        strlcpy((char*)ap_config.ap.password, value, sizeof(ap_config.ap.password));
        ESP_LOGI(TAG, "AP Password: %s", (char*)ap_config.ap.password);
    }
    FREE_AND_NULL(value);

    value = config_alloc_get_default(NVS_TYPE_STR, "ap_channel", STR(CONFIG_DEFAULT_AP_CHANNEL), 0);
    if (value != NULL) {
        ESP_LOGD(TAG, "Channel: %s", value);
        ap_config.ap.channel = atoi(value);
    }
    FREE_AND_NULL(value);

    ap_config.ap.authmode = AP_AUTHMODE;
    ap_config.ap.ssid_hidden = DEFAULT_AP_SSID_HIDDEN;
    ap_config.ap.max_connection = DEFAULT_AP_MAX_CONNECTIONS;
    ap_config.ap.beacon_interval = DEFAULT_AP_BEACON_INTERVAL;

    ESP_LOGD(TAG, "Auth Mode: %d", ap_config.ap.authmode);
    ESP_LOGD(TAG, "SSID Hidden: %d", ap_config.ap.ssid_hidden);
    ESP_LOGD(TAG, "Max Connections: %d", ap_config.ap.max_connection);
    ESP_LOGD(TAG, "Beacon interval: %d", ap_config.ap.beacon_interval);

    const char* msg = "Setting wifi mode as WIFI_MODE_APSTA";
    ESP_LOGD(TAG, "%s",msg);
    if ((err = esp_wifi_set_mode(WIFI_MODE_APSTA)) != ESP_OK) {
        ESP_LOGE(TAG, "%s. Error %s",msg, esp_err_to_name(err));
        return;
    }
    msg = "Setting wifi AP configuration for WIFI_IF_AP";
    ESP_LOGD(TAG, "%s", msg);
    if ((err = esp_wifi_set_config(WIFI_IF_AP, &ap_config)) != ESP_OK) /* stop AP DHCP server */
    {
        ESP_LOGE(TAG, "%s . Error %s", msg, esp_err_to_name(err));
        return;
    }

    msg = "Setting wifi bandwidth";
    ESP_LOGD(TAG, "%s (%d)", msg, DEFAULT_AP_BANDWIDTH);
    if ((err = esp_wifi_set_bandwidth(WIFI_IF_AP, DEFAULT_AP_BANDWIDTH)) != ESP_OK) /* stop AP DHCP server */
    {
        ESP_LOGE(TAG, "%s failed. Error %s", msg, esp_err_to_name(err));
        return;
    }

    msg = "Setting wifi power save";
    ESP_LOGD(TAG, "%s (%d)", msg, DEFAULT_STA_POWER_SAVE);

    if ((err = esp_wifi_set_ps(DEFAULT_STA_POWER_SAVE)) != ESP_OK) /* stop AP DHCP server */
    {
        ESP_LOGE(TAG, "%s failed. Error %s", msg, esp_err_to_name(err));
        return;
    }
    esp_netif_dhcps_start(wifi_netif);
    ESP_LOGD(TAG, "Done configuring Soft Access Point");
    dns_server_start();
}

void wifi_manager_filter_unique(wifi_ap_record_t* aplist, uint16_t* aps) {
    int total_unique;
    wifi_ap_record_t* first_free;
    total_unique = *aps;

    first_free = NULL;

    for (int i = 0; i < *aps - 1; i++) {
        wifi_ap_record_t* ap = &aplist[i];

        /* skip the previously removed APs */
        if (ap->ssid[0] == 0)
            continue;

        /* remove the identical SSID+authmodes */
        for (int j = i + 1; j < *aps; j++) {
            wifi_ap_record_t* ap1 = &aplist[j];
            if ((strcmp((const char*)ap->ssid, (const char*)ap1->ssid) == 0) &&
                (ap->authmode == ap1->authmode)) { /* same SSID, different auth mode is skipped */
                /* save the rssi for the display */
                if ((ap1->rssi) > (ap->rssi))
                    ap->rssi = ap1->rssi;
                /* clearing the record */
                memset(ap1, 0, sizeof(wifi_ap_record_t));
            }
        }
    }
    /* reorder the list so APs follow each other in the list */
    for (int i = 0; i < *aps; i++) {
        wifi_ap_record_t* ap = &aplist[i];
        /* skipping all that has no name */
        if (ap->ssid[0] == 0) {
            /* mark the first free slot */
            if (first_free == NULL)
                first_free = ap;
            total_unique--;
            continue;
        }
        if (first_free != NULL) {
            memcpy(first_free, ap, sizeof(wifi_ap_record_t));
            memset(ap, 0, sizeof(wifi_ap_record_t));
            /* find the next free slot */
            for (int j = 0; j < *aps; j++) {
                if (aplist[j].ssid[0] == 0) {
                    first_free = &aplist[j];
                    break;
                }
            }
        }
    }
    /* update the length of the list */
    *aps = total_unique;
}

char* wifi_manager_alloc_get_ap_list_json() {
    return cJSON_PrintUnformatted(accessp_cjson);
}
cJSON* wifi_manager_clear_ap_list_json(cJSON** old) {
    ESP_LOGV(TAG, "wifi_manager_clear_ap_list_json called");
    cJSON* root = wifi_manager_get_new_array_json(old);
    ESP_LOGV(TAG, "wifi_manager_clear_ap_list_json done");
    return root;
}
esp_err_t wifi_scan_done(queue_message* msg) {
    esp_err_t err = ESP_OK;
    /* As input param, it stores max AP number ap_records can hold. As output param, it receives the actual AP number this API returns.
				 * As a consequence, ap_num MUST be reset to MAX_AP_NUM at every scan */
    ESP_LOGD(TAG, "Getting AP list records");
    if ((err = esp_wifi_scan_get_ap_num(&ap_num)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to retrieve scan results count. Error %s", esp_err_to_name(err));
        return err;
    }

    if (ap_num > 0) {
        accessp_records = (wifi_ap_record_t*)malloc(sizeof(wifi_ap_record_t) * ap_num);
        if ((err = esp_wifi_scan_get_ap_records(&ap_num, accessp_records)) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to retrieve scan results list. Error %s", esp_err_to_name(err));
            return err;
        }
        /* make sure the http server isn't trying to access the list while it gets refreshed */
        ESP_LOGD(TAG, "Preparing to build ap JSON list");
        if (wifi_manager_lock_json_buffer(pdMS_TO_TICKS(1000))) {
            /* Will remove the duplicate SSIDs from the list and update ap_num */
            wifi_manager_filter_unique(accessp_records, &ap_num);
            wifi_manager_generate_access_points_json(&accessp_cjson);
            wifi_manager_unlock_json_buffer();
            ESP_LOGD(TAG, "Done building ap JSON list");

        } else {
            ESP_LOGE(TAG, "could not get access to json mutex in wifi_scan");
            err = ESP_FAIL;
        }
        free(accessp_records);
    } else {
        //
        ESP_LOGD(TAG, "No AP Found.  Emptying the list.");
        accessp_cjson = wifi_manager_get_new_array_json(&accessp_cjson);
    }
    return err;
}
bool is_wifi_up(){
    return wifi_netif!=NULL;
}
esp_err_t network_wifi_start_scan(queue_message* msg) {
    esp_err_t err = ESP_OK;
    ESP_LOGD(TAG, "MESSAGE: ORDER_START_WIFI_SCAN");
    if(!is_wifi_up()) {
        messaging_post_message(MESSAGING_WARNING, MESSAGING_CLASS_SYSTEM, "Wifi not started. Cannot scan");
        return ESP_FAIL;
    }
    /* if a scan is already in progress this message is simply ignored thanks to the WIFI_MANAGER_SCAN_BIT uxBit */
    if (!network_manager_is_flag_set(WIFI_MANAGER_SCAN_BIT)) {
        if ((err = esp_wifi_scan_start(&scan_config, false)) != ESP_OK) {
            ESP_LOGW(TAG, "Unable to start scan; %s ", esp_err_to_name(err));
            //						set_status_message(WARNING, "Wifi Connecting. Cannot start scan.");
            messaging_post_message(MESSAGING_WARNING, MESSAGING_CLASS_SYSTEM, "Scanning failed: %s", esp_err_to_name(err));
        } else {
            network_manager_set_flag(WIFI_MANAGER_SCAN_BIT);
        }
    } else {
        ESP_LOGW(TAG, "Scan already in progress!");
    }

    return err;
}
static void polling_STA(void* timer_id) {
    network_manager_async_connect(wifi_manager_get_wifi_sta_config());
}

void set_host_name() {
    esp_err_t err;
    ESP_LOGD(TAG, "Retrieving host name from nvs");
    char* host_name = (char*)config_alloc_get(NVS_TYPE_STR, "host_name");
    if (host_name == NULL) {
        ESP_LOGE(TAG, "Could not retrieve host name from nvs");
    } else {
        ESP_LOGD(TAG, "Setting host name to : %s", host_name);
        if ((err = tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, host_name)) != ESP_OK) {
            ESP_LOGE(TAG, "Unable to set host name. Error: %s", esp_err_to_name(err));
        }
        //		if((err=tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_AP, host_name)) !=ESP_OK){
        //			ESP_LOGE(TAG,  "Unable to set host name. Error: %s",esp_err_to_name(err));
        //		}
        free(host_name);
    }
}

esp_err_t network_wifi_connect(wifi_config_t * cfg){
    esp_err_t err = ESP_OK;
    ESP_LOGD(TAG, "network_wifi_connect");
    if(!is_wifi_up()) {
        messaging_post_message(MESSAGING_WARNING, MESSAGING_CLASS_SYSTEM, "Wifi not started. Cannot connect");
        return ESP_FAIL;
    }    
    tcpip_adapter_dhcp_status_t status;
    ESP_LOGD(TAG, "wifi_manager: Checking if DHCP client for STA interface is running");
    ESP_ERROR_CHECK_WITHOUT_ABORT(tcpip_adapter_dhcpc_get_status(TCPIP_ADAPTER_IF_STA, &status));
    if (status != TCPIP_ADAPTER_DHCP_STARTED) {
        ESP_LOGD(TAG, "wifi_manager: Start DHCP client for STA interface");
        ESP_ERROR_CHECK_WITHOUT_ABORT(tcpip_adapter_dhcpc_start(TCPIP_ADAPTER_IF_STA));
    }
    wifi_mode_t mode;
    /* update config to latest and attempt connection */
    esp_wifi_get_mode(&mode);
    if (WIFI_MODE_APSTA != mode && WIFI_MODE_STA != mode) {
        // the soft ap is not started, so let's set the WiFi mode to STA
        ESP_LOGD(TAG, "MESSAGE: network_wifi_connect_existing - setting mode WIFI_MODE_STA");
        if ((err = esp_wifi_set_mode(WIFI_MODE_STA)) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set wifi mode to STA. Error %s", esp_err_to_name(err));
            return err;
        }
    }
       
    if ((err = esp_wifi_set_config(WIFI_IF_STA, cfg)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set STA configuration. Error %s", esp_err_to_name(err));
        return err;
    }

    set_host_name();
    ESP_LOGI(TAG, "Wifi Connecting...");
    if ((err = esp_wifi_connect()) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initiate wifi connection. Error %s", esp_err_to_name(err));
        return err;
    }    
    return err;
}
void network_wifi_clear_config(){
    
    /* erase configuration */
    if (wifi_manager_config_sta) {
        ESP_LOGI(TAG, "Erasing WiFi Configuration.");
        memset(wifi_manager_config_sta, 0x00, sizeof(wifi_config_t));
        /* save NVS memory */
        network_wifi_save_sta_config();
    }

}


// esp_err_t network_wifi_disconnected(queue_message* msg) {
//     esp_err_t err = ESP_OK;
//     wifi_event_sta_disconnected_t disc_event;

//     // ESP_LOGD(TAG, "MESSAGE: EVENT_STA_DISCONNECTED");
//     // if (msg->param == NULL) {
//     //     ESP_LOGE(TAG, "MESSAGE: EVENT_STA_DISCONNECTED - expected parameter not found!");
//     // } else {
//     //     memcpy(&disc_event, (wifi_event_sta_disconnected_t*)msg->param, sizeof(disc_event));
//     //     free(msg->param);
//     //     ESP_LOGD(TAG, "MESSAGE: EVENT_STA_DISCONNECTED with Reason code: %d (%s)", disc_event.reason, get_disconnect_code_desc(disc_event.reason));
//     // }

//     /* this even can be posted in numerous different conditions
// 				 *
// 				 * 1. SSID password is wrong
// 				 * 2. Manual disconnection ordered
// 				 * 3. Connection lost
// 				 *
// 				 * Having clear understand as to WHY the event was posted is key to having an efficient wifi manager
// 				 *
// 				 * With wifi_manager, we determine:
// 				 *  If WIFI_MANAGER_REQUEST_STA_CONNECT_BIT is set, We consider it's a client that requested the connection.
// 				 *    When SYSTEM_EVENT_STA_DISCONNECTED is posted, it's probably a password/something went wrong with the handshake.
// 				 *
// 				 *  If WIFI_MANAGER_REQUEST_STA_CONNECT_BIT is set, it's a disconnection that was ASKED by the client (clicking disconnect in the app)
// 				 *    When SYSTEM_EVENT_STA_DISCONNECTED is posted, saved wifi is erased from the NVS memory.
// 				 *
// 				 *  If WIFI_MANAGER_REQUEST_STA_CONNECT_BIT and WIFI_MANAGER_REQUEST_STA_CONNECT_BIT are NOT set, it's a lost connection
// 				 *
// 				 *  In this version of the software, reason codes are not used. They are indicated here for potential future usage.
// 				 *
// 				 *  REASON CODE:
// 				 *  1		UNSPECIFIED
// 				 *  2		AUTH_EXPIRE					auth no longer valid, this smells like someone changed a password on the AP
// 				 *  3		AUTH_LEAVE
// 				 *  4		ASSOC_EXPIRE
// 				 *  5		ASSOC_TOOMANY				too many devices already connected to the AP => AP fails to respond
// 				 *  6		NOT_AUTHED
// 				 *  7		NOT_ASSOCED
// 				 *  8		ASSOC_LEAVE
// 				 *  9		ASSOC_NOT_AUTHED
// 				 *  10		DISASSOC_PWRCAP_BAD
// 				 *  11		DISASSOC_SUPCHAN_BAD
// 				 *	12		<n/a>
// 				 *  13		IE_INVALID
// 				 *  14		MIC_FAILURE
// 				 *  15		4WAY_HANDSHAKE_TIMEOUT		wrong password! This was personnaly tested on my home wifi with a wrong password.
// 				 *  16		GROUP_KEY_UPDATE_TIMEOUT
// 				 *  17		IE_IN_4WAY_DIFFERS
// 				 *  18		GROUP_CIPHER_INVALID
// 				 *  19		PAIRWISE_CIPHER_INVALID
// 				 *  20		AKMP_INVALID
// 				 *  21		UNSUPP_RSN_IE_VERSION
// 				 *  22		INVALID_RSN_IE_CAP
// 				 *  23		802_1X_AUTH_FAILED			wrong password?
// 				 *  24		CIPHER_SUITE_REJECTED
// 				 *  200		BEACON_TIMEOUT
// 				 *  201		NO_AP_FOUND
// 				 *  202		AUTH_FAIL
// 				 *  203		ASSOC_FAIL
// 				 *  204		HANDSHAKE_TIMEOUT
// 				 *
// 				 * */

//     /* reset saved sta IP */
//     wifi_manager_safe_reset_sta_ip_string();

//     if (network_manager_is_flag_set(WIFI_MANAGER_REQUEST_STA_CONNECT_BIT)) {
//         network_manager_clear_flag(WIFI_MANAGER_REQUEST_STA_CONNECT_BIT);
//         ESP_LOGW(TAG, "WiFi Disconnected while processing user connect request.  Wrong password?");
//         /* there are no retries when it's a user requested connection by design. This avoids a user hanging too much
// 					 * in case they typed a wrong password for instance. Here we simply clear the request bit and move on */
            

//         wifi_mode_t mode;
//         esp_wifi_get_mode(&mode);
//         if (WIFI_MODE_STA == mode) {
//             network_manager_set_flag(WIFI_MANAGER_REQUEST_STA_CONNECT_FAILED_BIT);
//             // if wifi was STA, attempt to reload the previous network connection
//             ESP_LOGW(TAG, "Attempting to restore previous network");
//             wifi_manager_send_message(ORDER_LOAD_AND_RESTORE_STA, NULL);
//         }
//     } else if (network_manager_is_flag_set(WIFI_MANAGER_REQUEST_DISCONNECT_BIT)) {
//         // ESP_LOGD(TAG, "WiFi disconnected by user");
//         // /* user manually requested a disconnect so the lost connection is a normal event. Clear the flag and restart the AP */
//         // network_manager_clear_flag(WIFI_MANAGER_REQUEST_DISCONNECT_BIT);
//         // wifi_manager_generate_ip_info_json(UPDATE_USER_DISCONNECT,  wifi_netif,false);
//         // /* erase configuration */
//         // if (wifi_manager_config_sta) {
//         //     ESP_LOGI(TAG, "Erasing WiFi Configuration.");
//         //     memset(wifi_manager_config_sta, 0x00, sizeof(wifi_config_t));
//         //     /* save NVS memory */
//         //     network_wifi_save_sta_config();
//         // }
//         // /* start SoftAP */
//         // ESP_LOGD(TAG, "Disconnect processing complete. Ordering an AP start.");
//         // wifi_manager_send_message(ORDER_START_AP, NULL);
//     } else {
//         /* lost connection ? */
//         // ESP_LOGE(TAG, "WiFi Connection lost.");
//         // messaging_post_message(MESSAGING_WARNING, MESSAGING_CLASS_SYSTEM, "WiFi Connection lost");
//         // wifi_manager_generate_ip_info_json(UPDATE_LOST_CONNECTION,  wifi_netif,false);

//         // if (retries < WIFI_MANAGER_MAX_RETRY) {
//         //     ESP_LOGD(TAG, "Issuing ORDER_CONNECT_STA to retry connection.");
//         //     retries++;
//         //     wifi_manager_send_message(ORDER_CONNECT_STA, (void*)CONNECTION_REQUEST_AUTO_RECONNECT);
//         // } else {
//         //     /* In this scenario the connection was lost beyond repair: kick start the AP! */
//         //     retries = 0;
//         //     wifi_mode_t mode;
//         //     ESP_LOGW(TAG, "All connect retry attempts failed.");

//         //     /* put us in softAP mode first */
//         //     esp_wifi_get_mode(&mode);
//         //     /* if it was a restore attempt connection, we clear the bit */
//         //     network_manager_clear_flag(WIFI_MANAGER_REQUEST_RESTORE_STA_BIT);

//         //     if (WIFI_MODE_APSTA != mode) {
//         //         STA_duration = STA_POLLING_MIN;
//         //         wifi_manager_send_message(ORDER_CONNECT_STA, (void*)CONNECTION_REQUEST_MAX_FAILED);
//         //     } else if (STA_duration < STA_POLLING_MAX) {
//         //         STA_duration *= 1.25;
//         //     }

//         //     xTimerChangePeriod(STA_timer, pdMS_TO_TICKS(STA_duration), portMAX_DELAY);
//         //     xTimerStart(STA_timer, portMAX_DELAY);
//         //     ESP_LOGD(TAG, "STA search slow polling of %d", STA_duration);
//         // }
//     }
//     return err;
// }


char* get_disconnect_code_desc(uint8_t reason) {
    switch (reason) {
        case 1:
            return "UNSPECIFIED";
            break;
        case 2:
            return "AUTH_EXPIRE";
            break;
        case 3:
            return "AUTH_LEAVE";
            break;
        case 4:
            return "ASSOC_EXPIRE";
            break;
        case 5:
            return "ASSOC_TOOMANY";
            break;
        case 6:
            return "NOT_AUTHED";
            break;
        case 7:
            return "NOT_ASSOCED";
            break;
        case 8:
            return "ASSOC_LEAVE";
            break;
        case 9:
            return "ASSOC_NOT_AUTHED";
            break;
        case 10:
            return "DISASSOC_PWRCAP_BAD";
            break;
        case 11:
            return "DISASSOC_SUPCHAN_BAD";
            break;
        case 12:
            return "<n/a>";
            break;
        case 13:
            return "IE_INVALID";
            break;
        case 14:
            return "MIC_FAILURE";
            break;
        case 15:
            return "4WAY_HANDSHAKE_TIMEOUT";
            break;
        case 16:
            return "GROUP_KEY_UPDATE_TIMEOUT";
            break;
        case 17:
            return "IE_IN_4WAY_DIFFERS";
            break;
        case 18:
            return "GROUP_CIPHER_INVALID";
            break;
        case 19:
            return "PAIRWISE_CIPHER_INVALID";
            break;
        case 20:
            return "AKMP_INVALID";
            break;
        case 21:
            return "UNSUPP_RSN_IE_VERSION";
            break;
        case 22:
            return "INVALID_RSN_IE_CAP";
            break;
        case 23:
            return "802_1X_AUTH_FAILED";
            break;
        case 24:
            return "CIPHER_SUITE_REJECTED";
            break;
        case 200:
            return "BEACON_TIMEOUT";
            break;
        case 201:
            return "NO_AP_FOUND";
            break;
        case 202:
            return "AUTH_FAIL";
            break;
        case 203:
            return "ASSOC_FAIL";
            break;
        case 204:
            return "HANDSHAKE_TIMEOUT";
            break;
        default:
            return "UNKNOWN";
            break;
    }
    return "";
}


static void network_manager_wifi_ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    ip_event_got_ip_t* s = NULL;
    tcpip_adapter_if_t index;
    esp_netif_ip_info_t* ip_info = NULL;

    if (event_base != IP_EVENT)
        return;
    switch (event_id) {
        case IP_EVENT_ETH_GOT_IP:
        case IP_EVENT_STA_GOT_IP:
            s = (ip_event_got_ip_t*)event_data;
            //tcpip_adapter_if_t index = s->if_index;
            network_manager_async_got_ip();
            ip_info = &s->ip_info;
            index = s->if_index;

            ESP_LOGI(TAG, "Got an IP address from interface #%i. IP=" IPSTR ", Gateway=" IPSTR ", NetMask=" IPSTR ", %s",
                     index,
                     IP2STR(&ip_info->ip),
                     IP2STR(&ip_info->gw),
                     IP2STR(&ip_info->netmask),
                     s->ip_changed ? "Address was changed" : "Address unchanged");
            
            break;
        case IP_EVENT_STA_LOST_IP:
            ESP_LOGD(TAG, "IP_EVENT_STA_LOST_IP");
            break;
        case IP_EVENT_AP_STAIPASSIGNED:
            ESP_LOGD(TAG, "IP_EVENT_AP_STAIPASSIGNED");
            break;
        case IP_EVENT_GOT_IP6:
            ESP_LOGD(TAG, "IP_EVENT_GOT_IP6");
            break;
        default:
            break;
    }
}