#include "esp_eth.h"
#include "network_ethernet.h"

static esp_eth_mac_t* mac_new(spi_device_handle_t spi_handle, eth_config_t* ethernet_config) {
#ifdef CONFIG_ETH_PHY_INTERFACE_RMII
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    mac_config.smi_mdc_gpio_num = ethernet_config->mdc;
    mac_config.smi_mdio_gpio_num = ethernet_config->mdio;
    mac_config.sw_reset_timeout_ms = 400;
    return esp_eth_mac_new_esp32(&mac_config);
#else
    return NULL;
#endif
}
static esp_eth_phy_t* phy_new(eth_config_t* ethernet_config) {
#ifdef CONFIG_ETH_PHY_INTERFACE_RMII    
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = 1;
    phy_config.reset_gpio_num = ethernet_config->rst;
    return esp_eth_phy_new_lan8720(&phy_config);
#else
    return NULL;
#endif    
}
static void init_config(eth_config_t* ethernet_config) {
#ifdef CONFIG_ETH_PHY_INTERFACE_RMII
#else
    return NULL;
#endif
}

static network_ethernet_driver_t LAN8720 = {

    .mac_new = mac_new,
    .phy_new = phy_new,
    .init_config = init_config,
#ifdef CONFIG_ETH_PHY_INTERFACE_RMII
    .valid = true,
#else
    .valid = false,
#endif
};
network_ethernet_driver_t* LAN8720_Detect(char* Driver) {
    if (!strcasestr(Driver, "LAN8720"))
        return NULL;
    return &LAN8720;
}
