#include "network_status.h"
#include <string.h>
#include "bt_app_core.h"
#include "esp_log.h"
#include "globdefs.h"
#include "lwip/inet.h"
#include "monitor.h"
#include "network_ethernet.h"
#include "network_wifi.h"
#include "platform_config.h"
#include "platform_esp32.h"
#include "tools.h"
#include "trace.h"
#ifndef CONFIG_SQUEEZELITE_ESP32_RELEASE_URL
#pragma message "Defaulting release url"
#define CONFIG_SQUEEZELITE_ESP32_RELEASE_URL "https://github.com/sle118/squeezelite-esp32/releases"
#endif
static const char TAG[] = "network_status";
SemaphoreHandle_t wifi_manager_json_mutex = NULL;
SemaphoreHandle_t wifi_manager_sta_ip_mutex = NULL;
char* release_url = NULL;
char* wifi_manager_sta_ip = NULL;
char* ip_info_json = NULL;
cJSON* ip_info_cjson = NULL;
static char lms_server_ip[IP4ADDR_STRLEN_MAX] = {0};
static uint16_t lms_server_port = 0;
static uint16_t lms_server_cport = 0;
static void (*chained_notify)(in_addr_t, u16_t, u16_t);
static void connect_notify(in_addr_t ip, u16_t hport, u16_t cport);
#define STA_IP_LEN sizeof(char) * IP4ADDR_STRLEN_MAX

void init_network_status() {
    chained_notify = server_notify;
    server_notify = connect_notify;
    ESP_LOGD(TAG, "init_network_status.  Creating mutexes");
    wifi_manager_json_mutex = xSemaphoreCreateMutex();
    wifi_manager_sta_ip_mutex = xSemaphoreCreateMutex();
    ip_info_json = NULL;
    ESP_LOGD(TAG, "init_network_status.  Creating status json structure");
    ip_info_cjson = wifi_manager_clear_ip_info_json(&ip_info_cjson);
    ESP_LOGD(TAG, "Getting release url ");
    char* release_url = (char*)config_alloc_get_default(NVS_TYPE_STR, "release_url", QUOTE(CONFIG_SQUEEZELITE_ESP32_RELEASE_URL), 0);
    if (release_url == NULL) {
        ESP_LOGE(TAG, "Unable to retrieve the release url from nvs");
    } else {
        ESP_LOGD(TAG, "Found release url %s", release_url);
    }
    ESP_LOGD(TAG, "About to set the STA IP String to 0.0.0.0");
    wifi_manager_sta_ip = (char*)malloc(STA_IP_LEN);
    wifi_manager_safe_update_sta_ip_string(NULL);
}
void destroy_network_status() {
    FREE_AND_NULL(release_url);
    FREE_AND_NULL(ip_info_json);
    FREE_AND_NULL(wifi_manager_sta_ip);
    cJSON_Delete(ip_info_cjson);
    vSemaphoreDelete(wifi_manager_json_mutex);
    wifi_manager_json_mutex = NULL;
    vSemaphoreDelete(wifi_manager_sta_ip_mutex);
    wifi_manager_sta_ip_mutex = NULL;
    ip_info_cjson = NULL;
}
cJSON* wifi_manager_get_new_json(cJSON** old) {
    ESP_LOGV(TAG, "wifi_manager_get_new_json called");
    cJSON* root = *old;
    if (root != NULL) {
        cJSON_Delete(root);
        *old = NULL;
    }
    ESP_LOGV(TAG, "wifi_manager_get_new_json done");
    return cJSON_CreateObject();
}

cJSON* wifi_manager_clear_ip_info_json(cJSON** old) {
    ESP_LOGV(TAG, "wifi_manager_clear_ip_info_json called");
    cJSON* root = wifi_manager_get_basic_info(old);
    ESP_LOGV(TAG, "wifi_manager_clear_ip_info_json done");
    return root;
}
void network_status_clear_ip() {
    if (wifi_manager_lock_json_buffer(portMAX_DELAY)) {
        ip_info_cjson = wifi_manager_clear_ip_info_json(&ip_info_cjson);
        wifi_manager_unlock_json_buffer();
    }
}
char* wifi_manager_alloc_get_ip_info_json() {
    return cJSON_PrintUnformatted(ip_info_cjson);
}

void wifi_manager_unlock_json_buffer() {
    ESP_LOGV(TAG, "Unlocking json buffer!");
    xSemaphoreGive(wifi_manager_json_mutex);
}

bool wifi_manager_lock_json_buffer(TickType_t xTicksToWait) {
    ESP_LOGV(TAG, "Locking json buffer");
    if (wifi_manager_json_mutex) {
        if (xSemaphoreTake(wifi_manager_json_mutex, xTicksToWait) == pdTRUE) {
            ESP_LOGV(TAG, "Json buffer locked!");
            return true;
        } else {
            ESP_LOGE(TAG, "Semaphore take failed. Unable to lock json buffer mutex");
            return false;
        }
    } else {
        ESP_LOGV(TAG, "Unable to lock json buffer mutex");
        return false;
    }
}

bool wifi_manager_lock_sta_ip_string(TickType_t xTicksToWait) {
    if (wifi_manager_sta_ip_mutex) {
        if (xSemaphoreTake(wifi_manager_sta_ip_mutex, xTicksToWait) == pdTRUE) {
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

void wifi_manager_unlock_sta_ip_string() {
    xSemaphoreGive(wifi_manager_sta_ip_mutex);
}

void wifi_manager_safe_update_sta_ip_string(esp_ip4_addr_t* ip4) {
    if (wifi_manager_lock_sta_ip_string(portMAX_DELAY)) {
        strcpy(wifi_manager_sta_ip, ip4 != NULL ? ip4addr_ntoa(ip4) : "0.0.0.0");
        ESP_LOGD(TAG, "Set STA IP String to: %s", wifi_manager_sta_ip);
        wifi_manager_unlock_sta_ip_string();
    }
}
void wifi_manager_safe_reset_sta_ip_string() {
    if (wifi_manager_lock_sta_ip_string(portMAX_DELAY)) {
        strcpy(wifi_manager_sta_ip, "0.0.0.0");
        ESP_LOGD(TAG, "Set STA IP String to: %s", wifi_manager_sta_ip);
        wifi_manager_unlock_sta_ip_string();
    }
}
char* wifi_manager_get_sta_ip_string() {
    return wifi_manager_sta_ip;
}
void set_lms_server_details(in_addr_t ip, u16_t hport, u16_t cport) {
    strncpy(lms_server_ip, inet_ntoa(ip), sizeof(lms_server_ip));
    lms_server_ip[sizeof(lms_server_ip) - 1] = '\0';
    ESP_LOGI(TAG, "LMS IP: %s, hport: %d, cport: %d", lms_server_ip, hport, cport);
    lms_server_port = hport;
    lms_server_cport = cport;
}
static void connect_notify(in_addr_t ip, u16_t hport, u16_t cport) {
    set_lms_server_details(ip, hport, cport);
    if (chained_notify)
        (*chained_notify)(ip, hport, cport);
    network_manager_async_update_status();
}
void wifi_manager_update_basic_info() {
    int32_t total_connected_time = 0;
    int64_t last_connected = 0;
    uint16_t num_disconnect = 0;
    network_wifi_get_stats(&total_connected_time, &last_connected, &num_disconnect);
    if (wifi_manager_lock_json_buffer(portMAX_DELAY)) {
        monitor_gpio_t* mgpio = get_jack_insertion_gpio();

        cJSON* voltage = cJSON_GetObjectItemCaseSensitive(ip_info_cjson, "Voltage");
        if (voltage) {
            cJSON_SetNumberValue(voltage, battery_value_svc());
        }
        cJSON* bt_status = cJSON_GetObjectItemCaseSensitive(ip_info_cjson, "bt_status");
        if (bt_status) {
            cJSON_SetNumberValue(bt_status, bt_app_source_get_a2d_state());
        }
        cJSON* bt_sub_status = cJSON_GetObjectItemCaseSensitive(ip_info_cjson, "bt_sub_status");
        if (bt_sub_status) {
            cJSON_SetNumberValue(bt_sub_status, bt_app_source_get_media_state());
        }
        cJSON* jack = cJSON_GetObjectItemCaseSensitive(ip_info_cjson, "Jack");
        if (jack) {
            jack->type = mgpio->gpio >= 0 && jack_inserted_svc() ? cJSON_True : cJSON_False;
        }
        cJSON* disconnect_count = cJSON_GetObjectItemCaseSensitive(ip_info_cjson, "disconnect_count");
        if (disconnect_count) {
            cJSON_SetNumberValue(disconnect_count, num_disconnect);
        }
        cJSON* avg_conn_time = cJSON_GetObjectItemCaseSensitive(ip_info_cjson, "avg_conn_time");
        if (avg_conn_time) {
            cJSON_SetNumberValue(avg_conn_time, num_disconnect > 0 ? (total_connected_time / num_disconnect) : 0);
        }
        if (lms_server_cport > 0) {
            cJSON* value = cJSON_GetObjectItemCaseSensitive(ip_info_cjson, "lms_cport");
            if (value) {
                cJSON_SetNumberValue(value, lms_server_cport);
            } else {
                cJSON_AddNumberToObject(ip_info_cjson, "lms_cport", lms_server_cport);
            }
        }

        if (lms_server_port > 0) {
            cJSON* value = cJSON_GetObjectItemCaseSensitive(ip_info_cjson, "lms_port");
            if (value) {
                cJSON_SetNumberValue(value, lms_server_port);
            } else {
                cJSON_AddNumberToObject(ip_info_cjson, "lms_port", lms_server_port);
            }
        }

        if (strlen(lms_server_ip) > 0) {
            cJSON* value = cJSON_GetObjectItemCaseSensitive(ip_info_cjson, "lms_ip");
            if (!value) {
                // only create if it does not exist. Since we're creating a reference
                // to a char buffer, updates to cJSON aren't needed
                cJSON_AddItemToObject(ip_info_cjson, "lms_ip", cJSON_CreateStringReference(lms_server_ip));
            }
        }
        if (network_ethernet_enabled()) {
            cJSON* eth = cJSON_GetObjectItemCaseSensitive(ip_info_cjson, "eth_up");
            if (eth) {
                eth->type = network_ethernet_is_up() ? cJSON_True : cJSON_False;
            }
        }
        wifi_manager_unlock_json_buffer();
    }
}
cJSON* network_status_update_string(cJSON** root, const char* key, const char* value) {
    if (*root == NULL) {
        *root = cJSON_CreateObject();
    }

    if (!key || !value || strlen(key) == 0)
        return *root;
    cJSON* cjsonvalue = cJSON_GetObjectItemCaseSensitive(*root, key);
    if (cjsonvalue && strcasecmp(cJSON_GetStringValue(cjsonvalue), value) != 0) {
        ESP_LOGD(TAG, "Value %s changed from %s to %s", key, cJSON_GetStringValue(cjsonvalue), value);
        cJSON_SetValuestring(cjsonvalue, value);
    } else {
        cJSON_AddItemToObject(*root, key, cJSON_CreateString(value));
    }
    return *root;
}
cJSON* network_status_update_number(cJSON** root, const char* key, int value) {
    if (wifi_manager_lock_json_buffer(portMAX_DELAY)) {
        if (*root == NULL) {
            *root = cJSON_CreateObject();
        }

        if (key && value && strlen(key) != 0) {
            cJSON* cjsonvalue = cJSON_GetObjectItemCaseSensitive(*root, key);
            if (cjsonvalue) {
                cJSON_SetNumberValue(cjsonvalue, value);
            } else {
                cJSON_AddNumberToObject(*root, key, value);
            }
        }
        wifi_manager_unlock_json_buffer();
    } else {
        ESP_LOGW(TAG, "Unable to lock status json buffer. ");
    }
    return *root;
}
cJSON* network_status_update_float(cJSON** root, const char* key, float value) {
    if (wifi_manager_lock_json_buffer(portMAX_DELAY)) {
        if (*root == NULL) {
            *root = cJSON_CreateObject();
        }

        if (key && strlen(key) != 0) {
            cJSON* cjsonvalue = cJSON_GetObjectItemCaseSensitive(*root, key);
            if (cjsonvalue) {
                cJSON_SetNumberValue(cjsonvalue, value);
            } else {
                cJSON_AddNumberToObject(*root, key, value);
            }
        }
        wifi_manager_unlock_json_buffer();
    } else {
        ESP_LOGW(TAG, "Unable to lock status json buffer. ");
    }
    return *root;
}
cJSON* network_status_update_bool(cJSON** root, const char* key, bool value) {
    if (wifi_manager_lock_json_buffer(portMAX_DELAY)) {
        if (*root == NULL) {
            *root = cJSON_CreateObject();
        }

        if (key && strlen(key) != 0) {
            cJSON* cjsonvalue = cJSON_GetObjectItemCaseSensitive(*root, key);
            if (cjsonvalue) {
                cjsonvalue->type = value ? cJSON_True : cJSON_False;
            } else {
                cJSON_AddBoolToObject(*root, key, value);
            }
        }
        wifi_manager_unlock_json_buffer();
    } else {
        ESP_LOGW(TAG, "Unable to lock status json buffer. ");
    }
    return *root;
}
cJSON* wifi_manager_get_basic_info(cJSON** old) {
    int32_t total_connected_time = 0;
    int64_t last_connected = 0;
    uint16_t num_disconnect = 0;
    network_wifi_get_stats(&total_connected_time, &last_connected, &num_disconnect);

    monitor_gpio_t* mgpio = get_jack_insertion_gpio();
    const esp_app_desc_t* desc = esp_ota_get_app_description();
    ESP_LOGV(TAG, "wifi_manager_get_basic_info called");
    cJSON* root = network_status_update_string(&root, "project_name", desc->project_name);
#ifdef CONFIG_FW_PLATFORM_NAME
    root = network_status_update_string(&root, "platform_name", CONFIG_FW_PLATFORM_NAME);
#endif
    root = network_status_update_string(&root, "version", desc->version);
    if (release_url != NULL)
        root = network_status_update_string(&root, "release_url", release_url);
    root = network_status_update_number(&root, "recovery", is_recovery_running ? 1 : 0);
    root = network_status_update_bool(&root, "Jack", mgpio->gpio >= 0 && jack_inserted_svc());
    root = network_status_update_float(&root, "Voltage", battery_value_svc());
    root = network_status_update_number(&root, "disconnect_count", num_disconnect);
    root = network_status_update_float(&root, "avg_conn_time", num_disconnect > 0 ? (total_connected_time / num_disconnect) : 0);
    root = network_status_update_number(&root, "bt_status", bt_app_source_get_a2d_state());
    root = network_status_update_number(&root, "bt_sub_status", bt_app_source_get_media_state());

#if CONFIG_I2C_LOCKED
    root = network_status_update_bool(&root, "is_i2c_locked", true);
#else
    root = network_status_update_bool(&root, "is_i2c_locked", false);
#endif
    if (network_ethernet_enabled()) {
        root = network_status_update_bool(&root, "eth_up", network_ethernet_is_up());
    }
    ESP_LOGV(TAG, "wifi_manager_get_basic_info done");
    return root;
}

void wifi_manager_generate_ip_info_json(update_reason_code_t update_reason_code) {
    ESP_LOGD(TAG, "wifi_manager_generate_ip_info_json called");

    if (wifi_manager_lock_json_buffer(portMAX_DELAY)) {
        /* generate the connection info with success */

        ip_info_cjson = wifi_manager_get_basic_info(&ip_info_cjson);
        wifi_manager_unlock_json_buffer();
    } else {
        ESP_LOGW(TAG, "Unable to lock status json buffer. ");
    }
    ip_info_cjson = network_status_update_number(&ip_info_cjson, "urc", update_reason_code);
    if (update_reason_code == UPDATE_CONNECTION_OK || update_reason_code == UPDATE_ETHERNET_CONNECTED) {
        /* rest of the information is copied after the ssid */
        tcpip_adapter_ip_info_t ip_info;
        esp_netif_get_ip_info(network_wifi_get_interface(), &ip_info);

        network_status_update_string(&ip_info_cjson, "ip", ip4addr_ntoa((ip4_addr_t*)&ip_info.ip));
        network_status_update_string(&ip_info_cjson, "netmask", ip4addr_ntoa((ip4_addr_t*)&ip_info.netmask));
        network_status_update_string(&ip_info_cjson, "gw", ip4addr_ntoa((ip4_addr_t*)&ip_info.gw));
        wifi_mode_t mode;
        if (esp_wifi_get_mode(&mode) == ESP_OK && (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA)) {
            /* wifi is active, and associated to an AP */
            wifi_ap_record_t ap;
            esp_wifi_sta_get_ap_info(&ap);
            network_status_update_string(&ip_info_cjson, "ssid", ((char*)ap.ssid));
            network_status_update_number(&ip_info_cjson, "rssi", ap.rssi);
        }
        if (network_ethernet_is_up()) {
            esp_netif_get_ip_info(network_ethernet_get_interface(), &ip_info);
            cJSON* ethernet_ip = cJSON_CreateObject();
            cJSON_AddItemToObject(ethernet_ip, "ip", cJSON_CreateString(ip4addr_ntoa((ip4_addr_t*)&ip_info.ip)));
            cJSON_AddItemToObject(ethernet_ip, "netmask", cJSON_CreateString(ip4addr_ntoa((ip4_addr_t*)&ip_info.netmask)));
            cJSON_AddItemToObject(ethernet_ip, "gw", cJSON_CreateString(ip4addr_ntoa((ip4_addr_t*)&ip_info.gw)));
            cJSON_AddItemToObject(ip_info_cjson, "eth", ethernet_ip);
        }
    } else {
        cJSON_DeleteItemFromObjectCaseSensitive(ip_info_cjson, "ip");
        cJSON_DeleteItemFromObjectCaseSensitive(ip_info_cjson, "netmask");
        cJSON_DeleteItemFromObjectCaseSensitive(ip_info_cjson, "gw");
        cJSON_DeleteItemFromObjectCaseSensitive(ip_info_cjson, "rssi");
        cJSON_DeleteItemFromObjectCaseSensitive(ip_info_cjson, "ssid");
        cJSON_DeleteItemFromObjectCaseSensitive(ip_info_cjson, "eth");
    }
    ESP_LOGV(TAG, "wifi_manager_generate_ip_info_json done");
}
