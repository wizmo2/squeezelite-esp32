#pragma once

#include "network_manager.h"
#include "accessors.h"
#include <string.h>
#ifdef __cplusplus
extern "C" {

#endif
typedef struct {
    bool valid;
    eth_config_t eth_config;
    esp_eth_mac_t* (*mac_new)(spi_device_handle_t spi_handle, eth_config_t * eth_config);
    esp_eth_phy_t *(*phy_new)( eth_config_t* eth_config);
    void (*init_config)(eth_config_t * eth_config);
} network_ethernet_driver_t;
typedef network_ethernet_driver_t* network_ethernet_detect_func_t(const char* Driver);

void destroy_network_ethernet();
void init_network_ethernet();
bool network_ethernet_wait_for_link(uint16_t max_wait_ms);

void network_ethernet_start_timer();
bool network_ethernet_is_up();
bool network_ethernet_enabled();
esp_netif_t *network_ethernet_get_interface();
#ifdef __cplusplus
}

#endif