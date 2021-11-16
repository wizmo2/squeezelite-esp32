#pragma once
#ifdef __cplusplus
extern "C" {

#endif

#include "esp_wifi.h"
#include "esp_wifi_types.h"


#define STA_POLLING_MIN (15 * 1000)
#define STA_POLLING_MAX (10 * 60 * 1000)

//enum class state { idle, stopped, started, running };

//enum class trigger { start, stop, set_speed, halt };
typedef enum  {
    t_link_up,
    t_link_down,
    t_configure,
    t_got_ip,
    t_disconnect,
    t_next,
    t_start,
    t_scan,
    t_fail,
    t_success,
    t_scan_done,
    t_connect,
    t_reboot,
    t_reboot_url,
    t_lost_connection,
    t_update_status
} trig_t;

typedef enum  { 
    instantiated,
    initializing,
    global,
    eth_starting,
    eth_active,
    eth_active_linkup,
    eth_active_connected,
    eth_active_linkdown,
    wifi_up,
    wifi_initializing,
    wifi_connecting_scanning,
    wifi_connecting,
    wifi_connected,
    wifi_disconnecting,
    wifi_user_disconnected,
    wifi_connected_waiting_for_ip,
    wifi_connected_scanning,
    wifi_lost_connection,
    wifi_ap_mode,
    wifi_ap_mode_scanning,
    wifi_ap_mode_scan_done,
    wifi_ap_mode_connecting,
    wifi_ap_mode_connected,
    system_rebooting
} state_t;
typedef enum {
	OTA,
	RECOVERY,
	RESTART,
} reboot_type_t;
typedef struct {
    trig_t trigger;
    union 
    {
        wifi_config_t* wifi_config;
        reboot_type_t rtype;
        char * strval;
        wifi_event_sta_disconnected_t * disconnected_event;
    } ;
	
    
} queue_message;

bool network_manager_event_simple(trig_t trigger);
bool network_manager_event(trig_t trigger, void* param);
bool network_t_connect_event(wifi_config_t* wifi_config);
bool network_t_link_event(bool link_up);
bool network_manager_async_event(trig_t trigger, void* param);
bool network_manager_async(trig_t trigger);
bool network_manager_async_fail();
bool network_manager_async_success();
bool network_manager_async_link_up();
bool network_manager_async_link_down();
bool network_manager_async_configure();
bool network_manager_async_got_ip();
bool network_manager_async_next();
bool network_manager_async_start();
bool network_manager_async_scan();
bool network_manager_async_scan_done();
bool network_manager_async_connect(wifi_config_t* wifi_config);
bool network_manager_async_lost_connection(wifi_event_sta_disconnected_t * disconnected_event);
bool network_manager_async_reboot(reboot_type_t rtype);
void network_manager_reboot_ota(char* url);
bool network_manager_async_disconnect();
bool network_manager_async_update_status();
/**
 * Allocate heap memory for the wifi manager and start the wifi_manager RTOS task
 */
void network_manager_start();
#ifdef __cplusplus
}
#endif