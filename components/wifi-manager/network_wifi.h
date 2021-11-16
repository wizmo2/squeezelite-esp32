#pragma once
#include "network_manager.h"
#ifdef __cplusplus
extern "C" {

#endif
void init_network_wifi();
void destroy_network_wifi(); 
/**
 * @brief saves the current STA wifi config to flash ram storage.
 */
esp_err_t network_wifi_save_sta_config();


/**
 * @brief fetch a previously STA wifi config in the flash ram storage.
 * @return true if a previously saved config was found, false otherwise.
 */
bool wifi_manager_fetch_wifi_sta_config();

wifi_config_t* wifi_manager_get_wifi_sta_config();

/**
 * @brief Registers handler for wifi and ip events
 */
void wifi_manager_register_handlers();

/**
 * @brief Generates the list of access points after a wifi scan.
 * @note This is not thread-safe and should be called only if wifi_manager_lock_json_buffer call is successful.
 */
void wifi_manager_generate_access_points_json(cJSON ** ap_list);

/**
 * @brief Clear the list of access points.
 * @note This is not thread-safe and should be called only if wifi_manager_lock_json_buffer call is successful.
 */
void wifi_manager_clear_access_points_json();


void wifi_manager_config_ap();

void wifi_manager_filter_unique( wifi_ap_record_t * aplist, uint16_t * aps);
esp_err_t wifi_scan_done(queue_message *msg);
esp_err_t network_wifi_start_scan(queue_message *msg);
esp_err_t network_wifi_load_restore(queue_message *msg);
esp_err_t network_wifi_order_connect(queue_message *msg);
esp_err_t network_wifi_disconnected(queue_message *msg);
esp_err_t network_wifi_start_ap(queue_message *msg);
bool wifi_manager_load_wifi_sta_config(wifi_config_t* config );
esp_err_t network_wifi_handle_event(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
esp_err_t network_wifi_connect(wifi_config_t * cfg);
bool is_wifi_up();
void network_wifi_clear_config();
esp_netif_t *network_wifi_get_interface();
bool network_wifi_sta_config_changed();
void network_wifi_get_stats( int32_t * ret_total_connected_time, int64_t * ret_last_connected, uint16_t * ret_num_disconnect);

#ifdef __cplusplus
}
#endif