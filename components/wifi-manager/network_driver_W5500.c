#include "esp_eth.h"
#include "network_ethernet.h"

static esp_eth_mac_t* mac_new(spi_device_handle_t spi_handle, eth_config_t* ethernet_config) {
#ifdef CONFIG_ETH_SPI_ETHERNET_W5500
    eth_w5500_config_t eth_config = ETH_W5500_DEFAULT_CONFIG(spi_handle);
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    // we assume that isr has been installed already
    eth_config.int_gpio_num = ethernet_config->intr;
    return esp_eth_mac_new_w5500(&eth_config, &mac_config);
#else
    return NULL;
#endif
}
static esp_eth_phy_t* phy_new(eth_config_t* ethernet_config) {
#ifdef CONFIG_ETH_SPI_ETHERNET_W5500
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = 1;
    phy_config.reset_gpio_num = ethernet_config->rst;
    return esp_eth_phy_new_w5500(&phy_config);
#else
    return NULL;
#endif
}

static void init_config(eth_config_t* ethernet_config) {
}

static network_ethernet_driver_t W5500 = {
    .mac_new = mac_new,
    .phy_new = phy_new,
    .init_config = init_config,
#ifdef CONFIG_ETH_SPI_ETHERNET_W5500
    .valid = true,
#else
    .valid = false,
#endif
};
network_ethernet_driver_t* W5500_Detect(char* Driver, network_ethernet_driver_t* Device) {
    if (!strcasestr(Driver, "W5500"))
        return NULL;
    return &W5500;
}
