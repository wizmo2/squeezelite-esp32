#include "esp_eth.h"
#include "network_ethernet.h"

static EXT_RAM_ATTR network_ethernet_driver_t LAN8720;
static esp_err_t start(spi_device_handle_t spi_handle, eth_config_t* ethernet_config) {
#ifdef CONFIG_ETH_PHY_INTERFACE_RMII
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();

    mac_config.smi_mdc_gpio_num = ethernet_config->mdc;
    mac_config.smi_mdio_gpio_num = ethernet_config->mdio;
    phy_config.phy_addr = 1;
    phy_config.reset_gpio_num = ethernet_config->rst;

    esp_eth_mac_t* mac = esp_eth_mac_new_esp32(&mac_config);
    esp_eth_phy_t* phy = esp_eth_phy_new_lan8720(&phy_config);
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    return esp_eth_driver_install(&config, &LAN8720.handle);
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

static void init_config(eth_config_t* ethernet_config) {
    LAN8720.start = start;
}

network_ethernet_driver_t* LAN8720_Detect(char* Driver) {
    if (!strcasestr(Driver, "LAN8720"))
        return NULL;
#ifdef CONFIG_ETH_PHY_INTERFACE_RMII
    LAN8720.valid = true;
#else
    LAN8720.valid = false;
#endif        
    LAN8720.rmii = true;
    LAN8720.spi = false;
    LAN8720.init_config = init_config;
    return &LAN8720;
}
