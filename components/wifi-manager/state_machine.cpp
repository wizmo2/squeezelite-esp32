#include <state_machine.hpp>

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include "cmd_system.h"
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
#include "esp_wifi_types.h"
#include "http_server_handlers.h"
#include "messaging.h"
#include "network_ethernet.h"
#include "network_status.h"
#include "network_wifi.h"
#include "state_machine.h"
#include "trace.h"
#include "dns_server.h"
BaseType_t network_manager_task;
/* objects used to manipulate the main queue of events */
QueueHandle_t network_manager_queue;

using namespace stateless;
using namespace std::placeholders;
static const char TAG[] = "network_manager";
static TaskHandle_t task_network_manager = NULL;
TimerHandle_t STA_timer = NULL;
uint32_t STA_duration;

static int32_t total_connected_time = 0;
static int64_t last_connected = 0;
static uint16_t num_disconnect = 0;
void network_wifi_get_stats(int32_t* ret_total_connected_time, int64_t* ret_last_connected, uint16_t* ret_num_disconnect) {
    *ret_total_connected_time = total_connected_time;
    *ret_last_connected = last_connected;
    *ret_num_disconnect = num_disconnect;
}
namespace {

std::ostream& operator<<(std::ostream& os, const state_t s) {
    //static const char* name[] = { "idle", "stopped", "started", "running" };
    static const char* name[] = {

        "initializing",
        "global",
        "eth_starting",
        "eth_active",
        "eth_active_linkup",
        "eth_active_connected",
        "eth_active_linkdown",
        "wifi_up",
        "wifi_initializing",
        "network_manager_async",
        "wifi_connecting_scanning",
        "wifi_connecting",
        "wifi_connected",
        "wifi_disconnecting",
        "wifi_user_disconnected",
        "wifi_connected_waiting_for_ip",
        "wifi_connected_scanning",
        "wifi_lost_connection",
        "wifi_ap_mode",
        "wifi_ap_mode_scanning",
        "wifi_ap_mode_scan_done",
        "wifi_ap_mode_connecting",
        "wifi_ap_mode_connected",
        "system_rebooting"};
    os << name[(int)s];
    return os;
}

const char* trigger_to_string(trig_t trigger) {
    switch (trigger) {
        ENUM_TO_STRING(t_link_up);
        ENUM_TO_STRING(t_link_down);
        ENUM_TO_STRING(t_configure);
        ENUM_TO_STRING(t_got_ip);
        ENUM_TO_STRING(t_next);
        ENUM_TO_STRING(t_start);
        ENUM_TO_STRING(t_scan);
        ENUM_TO_STRING(t_fail);
        ENUM_TO_STRING(t_success);
        ENUM_TO_STRING(t_scan_done);
        ENUM_TO_STRING(t_connect);
        ENUM_TO_STRING(t_reboot);
        ENUM_TO_STRING(t_reboot_url);
        ENUM_TO_STRING(t_lost_connection);
        ENUM_TO_STRING(t_update_status);
        ENUM_TO_STRING(t_disconnect);
        default:

            break;
    }
    return "Unknown trigger";
}
std::ostream& operator<<(std::ostream& os, const trig_t & t) {
    //static const char* name[] = { "start", "stop", "set_speed", "halt" };
    os << trigger_to_string(t);
    return os;
}

}  // namespace

// namespace stateless
// {

//   template<> void print_state<state>(std::ostream& os, const state& s)
//   { os << s; }

//   template<> void print_trigger<trigger_t_t(std::ostream& os, const trigger& t)
//   { os << t; }

// }

// namespace
// {

// class network_manager
// {
// public:
//   network_manager()
//   : sm_(state_t::initializing)
//   ,
// {

//}

//   sm_.configure(state_t::idle)
//     .permit(trig_t::t_start, state_t::started);

//   sm_.configure(state_t::stopped)
//     .on_entry([=](const TTransition&) { speed_ = 0; })
//     .permit(trig_t::t_halt, state_t::idle);

//   sm_.configure(state_t::started)
//     .permit(trig_t::t_set_speed, state_t::running)
//     .permit(trig_t::t_stop, state_t::stopped);

//   sm_.configure(state_t::running)
//     .on_entry_from(
//       set_speed_trigger_,
//       [=](const TTransition& t, int speed) { speed_ = speed; })
//     .permit(trig_t::t_stop, state_t::stopped)
//     .permit_reentry(trig_t::t_set_speed);

//   void start(int speed)
//   {
//     sm_.fire(trig_t::t_start);
//     set_speed(speed);
//   }

//   void stop()
//   {
//     sm_.fire(trig_t::t_stop);
//     sm_.fire(trig_t::t_halt);
//   }

//   void set_speed(int speed)
//   {
//     sm_.fire(set_speed_trigger_, speed);
//   }

//   std::string print() const
//   {
//     std::ostringstream oss;
//     print(oss);
//     return oss.str();
//   }

//   void print(std::ostream& os) const
//   {
//     os << "Motor " << sm_ << " speed = " << speed_;
//   }

// private:

//   //typedef std::shared_ptr<stateless::trigger_with_parameters<trigger_t, int>> TSetSpeedTrigger;

//   wifi_config_t * wifi_config;
//   TStateMachine sm_;
//   ;
//   ;
// };

// std::ostream& operator<<(std::ostream& os, const motor& m)
// {
//   m.print(os);
//   return os;
// }

//}
namespace {

typedef state_machine<state_t, trig_t> TStateMachine;
typedef TStateMachine::TTransition TTransition;
typedef std::shared_ptr<stateless::trigger_with_parameters<trig_t, wifi_config_t*>> TConnectTrigger;
typedef std::shared_ptr<stateless::trigger_with_parameters<trig_t, reboot_type_t>> TRebootTrigger;
typedef std::shared_ptr<stateless::trigger_with_parameters<trig_t, char*>> TRebootOTATrigger;
typedef std::shared_ptr<stateless::trigger_with_parameters<trig_t, wifi_event_sta_disconnected_t*>> TDisconnectTrigger;

};  // namespace

class NetworkManager {
   public:
    NetworkManager()
        : sm_(state_t::instantiated),
          connect_trigger_(sm_.set_trigger_parameters<wifi_config_t*>(trig_t::t_connect)),
          reboot_trigger_(sm_.set_trigger_parameters<reboot_type_t>(trig_t::t_reboot)),
          reboot_ota_(sm_.set_trigger_parameters<char*>(trig_t::t_reboot)),
          disconnected_(sm_.set_trigger_parameters<wifi_event_sta_disconnected_t*>(trig_t::t_lost_connection)) {
        sm_.configure(state_t::instantiated)
            .permit(trig_t::t_start, state_t::initializing);
        sm_.configure(state_t::global)
            .permit(trig_t::t_reboot, state_t::system_rebooting)
            .permit(trig_t::t_reboot_url, state_t::system_rebooting)
            .permit_reentry(trig_t::t_update_status)
            .on_entry_from(trig_t::t_update_status,
                           [=](const TTransition& t) {
                                wifi_manager_update_basic_info();
                           });
        sm_.configure(state_t::system_rebooting)
            .on_entry_from(reboot_ota_, [=](const TTransition& t, char* url) {
                if (url) {
                    start_ota(url, NULL, 0);
                    free(url);
                }
            })
            .on_entry_from(reboot_trigger_, [=](const TTransition& t, reboot_type_t reboot_type) {
                switch (reboot_type) {
                    case reboot_type_t::OTA:
                        ESP_LOGD(TAG, "Calling guided_restart_ota.");
                        guided_restart_ota();
                        break;
                    case reboot_type_t::RECOVERY:
                        ESP_LOGD(TAG, "Calling guided_factory.");
                        guided_factory();
                        break;
                    case reboot_type_t::RESTART:
                        ESP_LOGD(TAG, "Calling simple_restart.");
                        simple_restart();
                        break;

                    default:
                        break;
                }
            });

        sm_.configure(state_t::initializing)
            .on_entry([=](const TTransition& t) {
                /* memory allocation */
                ESP_LOGD(TAG, "network_manager_start.  Creating message queue");
                network_manager_queue = xQueueCreate(3, sizeof(queue_message));
                ESP_LOGD(TAG, "network_manager_start.  Allocating memory for callback functions registration");
                // todo:  allow registration of event callbacks
                ESP_LOGD(TAG, "Initializing tcp_ip adapter");
                esp_netif_init();
                ESP_LOGD(TAG, "Creating the default event loop");
                ESP_ERROR_CHECK(esp_event_loop_create_default());
                init_network_status();
                ESP_LOGD(TAG, "Initializing network. done");

                /* start wifi manager task */
                ESP_LOGD(TAG, "Creating network manager task");
                network_manager_task = xTaskCreate(&network_manager, "network_manager", 4096, NULL, WIFI_MANAGER_TASK_PRIORITY, &task_network_manager);
                // send a message to start the connections
            })
            .permit(trig_t::t_start, state_t::eth_starting);
        sm_.configure(state_t::eth_starting)
            .on_entry([=](const TTransition& t) {
                /* start http server */
                http_server_start();
                ESP_LOGD(TAG, "network_manager task started. Configuring Network Interface");
                init_network_ethernet();
            })
            .permit(trig_t::t_fail, state_t::wifi_initializing)
            .permit(trig_t::t_success, state_t::eth_active);

        sm_.configure(state_t::eth_active)
            .permit(trig_t::t_link_up, state_t::eth_active_linkup)
            .permit(trig_t::t_got_ip, state_t::eth_active_connected)
            .permit(trig_t::t_link_down, state_t::eth_active_linkdown)
            .permit(trig_t::t_fail, state_t::wifi_initializing);

        sm_.configure(state_t::eth_active_linkup)
            .sub_state_of(state_t::eth_active)
            .on_entry([=](const TTransition& t) {
                // Anything we need to do on link becoming active?
            });
        sm_.configure(state_t::eth_active_linkdown)
            .sub_state_of(state_t::eth_active)
            .on_entry([=](const TTransition& t) {
                // If not connected after a certain time, start
                // wifi
            });
        sm_.configure(state_t::eth_active_connected)
            .sub_state_of(state_t::eth_active)
            .on_entry([=](const TTransition& t) {

            })
            .on_exit([=](const TTransition& t) {

            });
        sm_.configure(state_t::wifi_up)
            .on_entry([=](const TTransition& t) {

            })
            .on_exit([=](const TTransition& t) {

            });

        sm_.configure(state_t::wifi_initializing)
            .sub_state_of(state_t::wifi_up)
            .on_entry([=](const TTransition& t) {
                esp_err_t err = ESP_OK;
                ESP_LOGD(TAG, "network_wifi_load_restore");
                if (!is_wifi_up()) {
                    messaging_post_message(MESSAGING_WARNING, MESSAGING_CLASS_SYSTEM, "Wifi not started. Load Configuration");
                    return ESP_FAIL;
                }
                if (wifi_manager_fetch_wifi_sta_config()) {
                    ESP_LOGI(TAG, "Saved wifi found on startup. Will attempt to connect.");
                    network_manager_async_connect(wifi_manager_get_wifi_sta_config());
                } else {
                    /* no wifi saved: start soft AP! This is what should happen during a first run */
                    ESP_LOGD(TAG, "No saved wifi. Starting AP.");
                    network_manager_async_configure();
                }
                return err;
            })
            .permit(trig_t::t_configure, state_t::wifi_ap_mode)
            .permit(trig_t::t_connect, state_t::wifi_connecting_scanning);

        sm_.configure(state_t::wifi_connecting_scanning)
            .sub_state_of(state_t::wifi_up)
            .on_entry_from(connect_trigger_, [=](const TTransition& t, wifi_config_t* wifi_config) {
                if (network_wifi_connect(wifi_config) == ESP_OK) {
                    network_manager_async_connect(wifi_config);
                } else {
                    network_manager_async_fail();
                }

                // STA_duration = STA_POLLING_MIN;
                //     /* create timer for background STA connection */
                //     if (!STA_timer) {
                //         STA_timer = xTimerCreate("background STA", pdMS_TO_TICKS(STA_duration), pdFALSE, NULL, polling_STA);
                //     }

                // setup a timeout here.  On timeout,
                // check reconnect_attempts and
                // fire trig_t::t_scan if we have more attempts to try
                // fire trig_t::t_fail  otherwise
            })
            .permit_reentry_if(trig_t::t_scan, [&]() {
                return ++reconnect_attempt < 3;
            })
            .permit(trig_t::t_connect, state_t::wifi_connecting)
            .permit(trig_t::t_fail, state_t::wifi_lost_connection);

        sm_.configure(state_t::wifi_connecting)
            .on_entry_from(connect_trigger_, [=](const TTransition& t, wifi_config_t* wifi_config) {
                // setup a timeout here.  On timeout,
                // check reconnect_attempts and
                // fire trig_t::t_wifi_connecting_existing if we have more attempts to try
                // fire trig_t::t_fail  otherwise
            })
            .permit(trig_t::t_success, state_t::wifi_connected_waiting_for_ip)
            .permit(trig_t::t_got_ip, state_t::wifi_connected)
            .permit(trig_t::t_lost_connection, state_t::wifi_ap_mode);
        sm_.configure(state_t::wifi_connected_waiting_for_ip)
            .permit(trig_t::t_got_ip, state_t::wifi_connected);

        sm_.configure(state_t::wifi_ap_mode)
            .on_entry([=](const TTransition& t) {
                wifi_manager_config_ap();
                ESP_LOGD(TAG, "AP Starting, requesting wifi scan.");
                network_manager_async_scan();
            })
            .on_entry_from(disconnected_, [=](const TTransition& t, wifi_event_sta_disconnected_t* disconnected_event) {
                if(disconnected_event){
                    free(disconnected_event);
                }
            })
            .permit(trig_t::t_scan, state_t::wifi_ap_mode_scanning);

        sm_.configure(state_t::wifi_ap_mode_scanning)
            .on_entry([=](const TTransition& t) {
                // build a list of found AP
            })
            .permit(trig_t::t_scan_done, state_t::wifi_ap_mode_scan_done);

        sm_.configure(state_t::wifi_ap_mode_scan_done)
            .permit(trig_t::t_connect, state_t::wifi_ap_mode_connecting);
        sm_.configure(state_t::wifi_ap_mode_connecting)
            .on_entry_from(connect_trigger_, [=](const TTransition& t, wifi_config_t* wifi_config) {

            })
            .permit(trig_t::t_got_ip, state_t::wifi_ap_mode_connected)
            .permit(trig_t::t_fail, state_t::wifi_ap_mode);

        sm_.configure(state_t::wifi_ap_mode_connected)
            .on_entry([=](const TTransition& t) {

            })
            .permit(trig_t::t_success, state_t::wifi_connected);

        sm_.configure(state_t::wifi_connected)
            .on_entry([&](const TTransition& t) {
                last_connected = esp_timer_get_time();
                /* bring down DNS hijack */
                ESP_LOGD(TAG, "Stopping DNS.");
                dns_server_stop();
                if (network_wifi_sta_config_changed()) {
                    network_wifi_save_sta_config();
                }

                /* stop AP mode */
                esp_wifi_set_mode(WIFI_MODE_STA);
            })
            .permit_reentry(trig_t::t_scan_done)
            .permit(trig_t::t_lost_connection, state_t::wifi_lost_connection)
            .permit(trig_t::t_scan, state_t::wifi_connected_scanning)
            .permit(trig_t::t_disconnect, state_t::wifi_disconnecting)
            .permit(trig_t::t_connect, state_t::wifi_connecting);

        sm_.configure(state_t::wifi_disconnecting)
            .permit(trig_t::t_lost_connection, state_t::wifi_user_disconnected);
        sm_.configure(state_t::wifi_user_disconnected)
            .on_entry_from(disconnected_,
                           [=](const TTransition& t, wifi_event_sta_disconnected_t* disconnected_event) {
                               ESP_LOGD(TAG, "WiFi disconnected by user");
                               network_wifi_clear_config();
                               wifi_manager_generate_ip_info_json(UPDATE_USER_DISCONNECT);
                               network_manager_async_configure();
                               if (disconnected_event) {
                                   free(disconnected_event);
                               }
                           })
            .permit(trig_t::t_configure, state_t::wifi_ap_mode);
        sm_.configure(state_t::wifi_lost_connection)
            .on_entry_from(disconnected_,
                           [=](const TTransition& t, wifi_event_sta_disconnected_t* disconnected_event) {
                               ESP_LOGE(TAG, "WiFi Connection lost.");
                               messaging_post_message(MESSAGING_WARNING, MESSAGING_CLASS_SYSTEM, "WiFi Connection lost");
                               wifi_manager_generate_ip_info_json(UPDATE_LOST_CONNECTION);
                               if (last_connected > 0)
                                   total_connected_time += ((esp_timer_get_time() - last_connected) / (1000 * 1000));
                               last_connected = 0;
                               num_disconnect++;
                               ESP_LOGW(TAG, "Wifi disconnected. Number of disconnects: %d, Average time connected: %d", num_disconnect, num_disconnect > 0 ? (total_connected_time / num_disconnect) : 0);
                               //network_manager_async_connect(wifi_manager_get_wifi_sta_config());
                               if (retries < WIFI_MANAGER_MAX_RETRY) {
                                   ESP_LOGD(TAG, "Issuing ORDER_CONNECT_STA to retry connection.");
                                   retries++;
                                   network_manager_async_connect(wifi_manager_get_wifi_sta_config());
                                   free(disconnected_event);
                               } else {
                                   /* In this scenario the connection was lost beyond repair: kick start the AP! */
                                   retries = 0;
                                   wifi_mode_t mode;
                                   ESP_LOGW(TAG, "All connect retry attempts failed.");

                                   /* put us in softAP mode first */
                                   esp_wifi_get_mode(&mode);
                                   if (WIFI_MODE_APSTA != mode) {
                                       STA_duration = STA_POLLING_MIN;
                                       network_manager_async_lost_connection(disconnected_event);
                                   } else if (STA_duration < STA_POLLING_MAX) {
                                       STA_duration *= 1.25;
                                       free(disconnected_event);
                                   }
                                   /* keep polling for existing connection */
                                   xTimerChangePeriod(STA_timer, pdMS_TO_TICKS(STA_duration), portMAX_DELAY);
                                   xTimerStart(STA_timer, portMAX_DELAY);
                                   ESP_LOGD(TAG, "STA search slow polling of %d", STA_duration);
                               }
                           })
            .on_entry([=](const TTransition& t) {
                wifi_manager_safe_reset_sta_ip_string();
            })
            .permit(trig_t::t_connect, state_t::wifi_connecting)
            .permit(trig_t::t_lost_connection, state_t::wifi_ap_mode);
        // Register a callback for state transitions (the default does nothing).
        sm_.on_transition([](const TTransition& t) {
            std::cout << "transition from [" << t.source() << "] to ["
                      << t.destination() << "] via trigger [" << t.trigger() << "]"
                      << std::endl;
        });

        // Override the default behaviour of throwing when a trigger is unhandled.
        sm_.on_unhandled_trigger([](const state_t s, const trig_t  t) {
            std::cerr << "ignore unhandled trigger [" << t << "] in state [" << s
                      << "]" << std::endl;
        });
    }

   public:
    bool
    Allowed(trig_t  trigger) {
        if (!sm_.can_fire(trigger)) {
            ESP_LOGW(TAG, "Network manager might not be able to process trigger %s", trigger_to_string(trigger));
            return false;
        }
        return true;
    }
    bool Fire(trig_t  trigger) {
        try {
            sm_.fire(trigger);
            return true;
        } catch (const std::exception& e) {
            std::cerr << e.what() << '\n';
        }
        return false;
    }
    bool Event(queue_message& msg) {
        trig_t  trigger = msg.trigger;
        try {
            if (trigger == trig_t::t_connect) {
                sm_.fire(connect_trigger_, msg.wifi_config);
            } else if (trigger == trig_t::t_reboot) {
                sm_.fire(reboot_trigger_, msg.rtype);
            } else if (trigger == trig_t::t_reboot_url) {
                sm_.fire(reboot_ota_, msg.strval);
            } else if (trigger == trig_t::t_lost_connection) {
                sm_.fire(disconnected_, msg.disconnected_event);
            } else {
                sm_.fire(trigger);
            }
            return true;
        } catch (const std::exception& e) {
            std::cerr << e.what() << '\n';
            return false;
        }
    }

   private:
    uint8_t reconnect_attempt = 0;
    bool existing_connection = false;
    uint8_t retries = 0;
    TStateMachine sm_;
    TConnectTrigger connect_trigger_;
    TRebootTrigger reboot_trigger_;
    TRebootOTATrigger reboot_ota_;
    TDisconnectTrigger disconnected_;
};

NetworkManager nm;
void network_manager_start() {
    nm.Fire(trig_t::t_start);
}

bool network_manager_async(trig_t  trigger) {
    queue_message msg;
    msg.trigger = trigger;
    if (nm.Allowed(trigger)) {
        return xQueueSendToFront(network_manager_queue, &msg, portMAX_DELAY);
    }
    return false;
}
bool network_manager_async_fail() {
    return network_manager_async(trig_t::t_fail);
}
bool network_manager_async_success() {
    return network_manager_async(trig_t::t_success);
}

bool network_manager_async_link_up() {
    return network_manager_async(trig_t::t_link_up);
}
bool network_manager_async_link_down() {
    return network_manager_async(trig_t::t_link_down);
}
bool network_manager_async_configure() {
    return network_manager_async(trig_t::t_configure);
}
bool network_manager_async_got_ip() {
    return network_manager_async(trig_t::t_got_ip);
}
bool network_manager_async_next() {
    return network_manager_async(trig_t::t_next);
}
bool network_manager_async_start() {
    return network_manager_async(trig_t::t_start);
}
bool network_manager_async_scan() {
    return network_manager_async(trig_t::t_scan);
}

bool network_manager_async_update_status() {
    return network_manager_async(trig_t::t_update_status);
}

bool network_manager_async_disconnect() {
    return network_manager_async(trig_t::t_disconnect);
}

bool network_manager_async_scan_done() {
    return network_manager_async(trig_t::t_scan_done);
}
bool network_manager_async_connect(wifi_config_t* wifi_config) {
    queue_message msg;
    msg.trigger = trig_t::t_connect;
    msg.wifi_config = wifi_config;
    if (nm.Allowed(msg.trigger)) {
        return xQueueSendToFront(network_manager_queue, &msg, portMAX_DELAY);
    }
    return false;
}
bool network_manager_async_lost_connection(wifi_event_sta_disconnected_t* disconnected_event) {
    queue_message msg;
    msg.trigger = trig_t::t_lost_connection;
    msg.disconnected_event = disconnected_event;
    if (nm.Allowed(msg.trigger)) {
        return xQueueSendToFront(network_manager_queue, &msg, portMAX_DELAY);
    }
    return false;
}
bool network_manager_async_reboot(reboot_type_t rtype) {
    queue_message msg;
    msg.trigger = trig_t::t_reboot;
    msg.rtype = rtype;
    if (nm.Allowed(msg.trigger)) {
        return xQueueSendToFront(network_manager_queue, &msg, portMAX_DELAY);
    }
    return false;
}

void network_manager_reboot_ota(char* url) {
    queue_message msg;

    if (url == NULL) {
        msg.trigger = trig_t::t_reboot;
        msg.rtype = reboot_type_t::OTA;
    } else {
        msg.trigger = trig_t::t_reboot_url;
        msg.strval = strdup(url);
    }
    if (nm.Allowed(msg.trigger)) {
        xQueueSendToFront(network_manager_queue, &msg, portMAX_DELAY);
    }
    return;
}

//     switch (msg.code) {
//         case EVENT_SCAN_DONE:
//             err = wifi_scan_done(&msg);
//             /* callback */
//             if (cb_ptr_arr[msg.code]) {
//                 ESP_LOGD(TAG, "Invoking SCAN DONE callback");
//                 (*cb_ptr_arr[msg.code])(NULL);
//                 ESP_LOGD(TAG, "Done Invoking SCAN DONE callback");
//             }
//             break;
//         case ORDER_START_WIFI_SCAN:
//             err = network_wifi_start_scan(&msg);
//             /* callback */
//             if (cb_ptr_arr[msg.code])
//                 (*cb_ptr_arr[msg.code])(NULL);
//             break;

//         case ORDER_LOAD_AND_RESTORE_STA:
//             err = network_wifi_load_restore(&msg);
//             /* callback */
//             if (cb_ptr_arr[msg.code])
//                 (*cb_ptr_arr[msg.code])(NULL);

//             break;

//         case ORDER_CONNECT_STA:
//             err = network_wifi_order_connect(&msg);
//             /* callback */
//             if (cb_ptr_arr[msg.code])
//                 (*cb_ptr_arr[msg.code])(NULL);

//             break;

//         case EVENT_STA_DISCONNECTED:
//             err = network_wifi_disconnected(&msg);
//             /* callback */
//             if (cb_ptr_arr[msg.code])
//                 (*cb_ptr_arr[msg.code])(NULL);
//             break;

//         case ORDER_START_AP:
//             err = network_wifi_start_ap(&msg);
//             /* callback */
//             if (cb_ptr_arr[msg.code])
//                 (*cb_ptr_arr[msg.code])(NULL);
//             break;

//         case EVENT_GOT_IP:
//             ESP_LOGD(TAG, "MESSAGE: EVENT_GOT_IP");
//             /* save IP as a string for the HTTP server host */
//             //s->ip_info.ip.addr
//             ip_event_got_ip_t* event = (ip_event_got_ip_t*)msg.param;
//             wifi_manager_safe_update_sta_ip_string(&(event->ip_info.ip));
//             wifi_manager_generate_ip_info_json(network_manager_is_flag_set(WIFI_MANAGER_REQUEST_STA_CONNECT_FAILED_BIT)? UPDATE_FAILED_ATTEMPT_AND_RESTORE : UPDATE_CONNECTION_OK, event->esp_netif, event->ip_changed);
//             free(msg.param);
//             /* callback */
//             if (cb_ptr_arr[msg.code])
//                 (*cb_ptr_arr[msg.code])(NULL);
//             break;
//         case UPDATE_CONNECTION_OK:
//             messaging_post_message(MESSAGING_ERROR, MESSAGING_CLASS_SYSTEM, "UPDATE_CONNECTION_OK not implemented");
//             break;
//         case ORDER_DISCONNECT_STA:
//             ESP_LOGD(TAG, "MESSAGE: ORDER_DISCONNECT_STA. Calling esp_wifi_disconnect()");

//             /* precise this is coming from a user request */
//             xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_DISCONNECT_BIT);

//             /* order wifi discconect */
//             ESP_ERROR_CHECK(esp_wifi_disconnect());

//             /* callback */
//             if (cb_ptr_arr[msg.code])
//                 (*cb_ptr_arr[msg.code])(NULL);

//             break;

//         case ORDER_RESTART_OTA_URL:
//             ESP_LOGD(TAG, "Calling start_ota.");
//             start_ota(msg.param, NULL, 0);
//             free(msg.param);
//             break;

//         case ORDER_RESTART_RECOVERY:

//             break;
//         case ORDER_RESTART:
//             ESP_LOGD(TAG, "Calling simple_restart.");
//             simple_restart();
//             break;
//         case ORDER_UPDATE_STATUS:
//             ;
//             break;
//         case EVENT_ETH_TIMEOUT:
//             ESP_LOGW(TAG, "Ethernet connection timeout.  Rebooting with WiFi Active");
// 	network_manager_reboot(RESTART);
//             break;
// case EVENT_ETH_LINK_UP:
//             /* callback */
//             ESP_LOGD(TAG,"EVENT_ETH_LINK_UP message received");
//             if (cb_ptr_arr[msg.code])
//                 (*cb_ptr_arr[msg.code])(NULL);
// break;
// case EVENT_ETH_LINK_DOWN:
//             /* callback */
//             if (cb_ptr_arr[msg.code])
//                 (*cb_ptr_arr[msg.code])(NULL);
// break;
//         default:
//             break;

//     } /* end of switch/case */

// if (!network_ethernet_wait_for_link(500)) {
//     if(network_ethernet_enabled()){
//         ESP_LOGW(TAG, "Ethernet not connected. Starting Wifi");
//     }
//     init_network_wifi();
//     wifi_manager_send_message(ORDER_LOAD_AND_RESTORE_STA, NULL);
// }

void network_manager(void* pvParameters) {
    queue_message msg;
    esp_err_t err = ESP_OK;
    BaseType_t xStatus;
    network_manager_async(trig_t::t_start);

    /* main processing loop */
    for (;;) {
        xStatus = xQueueReceive(network_manager_queue, &msg, portMAX_DELAY);

        if (xStatus == pdPASS) {
            // pass the event to the sync processor
            nm.Event(msg);
        } /* end of if status=pdPASS */
    }     /* end of for loop */

    vTaskDelete(NULL);
}

void network_manager_destroy() {
    vTaskDelete(task_network_manager);
    task_network_manager = NULL;
    /* heap buffers */
    destroy_network_status();
    destroy_network_wifi();
    destroy_network_status();
    /* RTOS objects */
    vQueueDelete(network_manager_queue);
    network_manager_queue = NULL;
}