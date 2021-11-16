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

#include "network_manager.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "network_ethernet.h"
#include "network_status.h"
#include "network_wifi.h"
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
#include "platform_esp32.h"


#include "esp_system.h"
#include "freertos/FreeRTOS.h"

#include "freertos/task.h"
#include "esp_netif.h"

#include "cJSON.h"
#include "cmd_system.h"
#include "esp_app_format.h"

#include "esp_event.h"
#include "esp_ota_ops.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/ip4_addr.h"
#include "lwip/netdb.h"
#include "mdns.h"
#include "messaging.h"
#include "state_machine.h"

#include "platform_config.h"
#include "trace.h"

#include "accessors.h"
#include "globdefs.h"
#include "http_server_handlers.h"



#ifndef STR_OR_BLANK
#define STR_OR_BLANK(p) p == NULL ? "" : p
#endif

//EventGroupHandle_t wifi_manager_event_group;
void (**cb_ptr_arr)(void*) = NULL;

/* @brief tag used for ESP serial console messages */
//static const char TAG[] = "network_manager";

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

/* @brief task handle for the main wifi_manager task */








void wifi_manager_set_callback(message_code_t message_code, void (*func_ptr)(void*)) {
    if (cb_ptr_arr && message_code < MESSAGE_CODE_COUNT) {
        cb_ptr_arr[message_code] = func_ptr;
    }
}

