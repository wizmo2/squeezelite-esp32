#include "esp_eth.h"
#include "network_ethernet.h"

static esp_eth_mac_t* mac_new(spi_device_handle_t spi_handle, eth_config_t* ethernet_config) {
#ifdef CONFIG_ETH_SPI_ETHERNET_DM9051
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_dm9051_config_t eth_config = ETH_DM9051_DEFAULT_CONFIG(spi_handle);
    // we assume that isr has been installed already
    eth_config.int_gpio_num = ethernet_config->intr;
    return esp_eth_mac_new_dm9051(&eth_config, &mac_config);
#else
    return NULL;
#endif
}
static esp_eth_phy_t* phy_new(eth_config_t* ethernet_config) {
#ifdef CONFIG_ETH_SPI_ETHERNET_DM9051
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = 1;
    phy_config.reset_gpio_num = ethernet_config->rst;
    return esp_eth_phy_new_dm9051(&phy_config);
#else
    return NULL;
#endif
}
static void init_config(eth_config_t* ethernet_config) {
}

static network_ethernet_driver_t DM9051 = {
    .mac_new = mac_new,
    .phy_new = phy_new,
    .init_config = init_config,
    .valid = true,
};
network_ethernet_driver_t* DM9051_Detect(char* Driver) {
    if (!strcasestr(Driver, "DM9051"))
        return NULL;
    return &DM9051;
}
