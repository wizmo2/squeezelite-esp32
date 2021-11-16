#include "network_ethernet.h"
#include "freertos/timers.h"
#include "globdefs.h"
#include "messaging.h"
#include "network_status.h"
#include "platform_config.h"
#include "trace.h"
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

//#include "dnserver.h"

static char TAG[] = "network_ethernet";
TimerHandle_t ETH_timer;
esp_eth_handle_t eth_handle = NULL;
esp_netif_t* eth_netif = NULL;
EventGroupHandle_t ethernet_event_group;
const int LINK_UP_BIT = BIT0;

static const char* known_drivers[] = {"DM9051", "W5500", "LAN8720", NULL};
static network_ethernet_driver_t* network_driver = NULL;
extern network_ethernet_detect_func_t DM9051_Detect, W5500_Detect, LAN8720_Detect;
static network_ethernet_detect_func_t* drivers[] = {DM9051_Detect, W5500_Detect, LAN8720_Detect, NULL};
#define ETH_TIMEOUT_MS (30 * 1000)
static void network_manager_ethernet_ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

/****************************************************************************************
 *
 */
const char* network_ethernet_conf_get_driver_name(const char* driver) {
    for (uint8_t i = 0; known_drivers[i] != NULL && strlen(known_drivers[i]) > 0; i++) {
        if (strcasestr(driver, known_drivers[i])) {
            return known_drivers[i];
        }
    }
    return NULL;
}
/****************************************************************************************
 *
 */
bool network_ethernet_is_valid_driver(const char* driver) {
    return network_ethernet_conf_get_driver_name(driver) != NULL;
}

network_ethernet_driver_t* network_ethernet_driver_autodetect(const char* Driver) {
    if (!Driver)
        return NULL;

    for (int i = 0; drivers[i]; i++) {
        network_ethernet_driver_t* found_driver = drivers[i](Driver);
        if (found_driver) {
            ESP_LOGD(TAG, "Detected driver %s ", Driver);

            network_driver = found_driver;
            return found_driver;
        }
    }
    return NULL;
}

static void eth_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
esp_netif_t *network_ethernet_get_interface(){
    return eth_netif;
}

bool network_ethernet_is_up() {
    return (xEventGroupGetBits(ethernet_event_group) & LINK_UP_BIT)!=0;
}
bool network_ethernet_enabled() {
    return eth_handle != NULL;
}
bool network_ethernet_wait_for_link(uint16_t max_wait_ms){
    if(!network_ethernet_enabled()) return false;
	bool link_up=(xEventGroupGetBits(ethernet_event_group) & LINK_UP_BIT)!=0;
	if(!link_up){
		ESP_LOGD(TAG,"Waiting for Ethernet link to be established...");
	    link_up = (xEventGroupWaitBits(ethernet_event_group, LINK_UP_BIT,pdFALSE, pdTRUE, max_wait_ms / portTICK_PERIOD_MS)& LINK_UP_BIT)!=0;
	    if(!link_up){
	    	ESP_LOGW(TAG,"Ethernet Link timeout.");
	    }
	    else
	    {
	    	ESP_LOGI(TAG,"Ethernet Link Up!");
	    }
	}
    return link_up;
}

static void ETH_Timeout(void* timer_id);
void destroy_network_ethernet() {
}
static void set_host_name() {
    ESP_LOGE(TAG, "TODO: Find a way to set the host name here!");
    // esp_err_t err;
    // ESP_LOGD(TAG, "Retrieving host name from nvs");
    // char* host_name = (char*)config_alloc_get(NVS_TYPE_STR, "host_name");
    // if (host_name == NULL) {
    //     ESP_LOGE(TAG, "Could not retrieve host name from nvs");
    // } else {
    //     if (!network_ethernet_enabled()) {
    //         ESP_LOGE(TAG, "Cannot set name on a disabled interface");
    //     } else {
    //         ESP_LOGD(TAG, "Setting host name to : %s", host_name);
    //         if ((err = esp_netif_set_hostname(eth_handle, host_name)) != ESP_OK) {
    //             ESP_LOGE(TAG, "Unable to set host name. Error: %s", esp_err_to_name(err));
    //         }
    //         ESP_LOGD(TAG, "Done setting host name to : %s", host_name);
    //     }

    //     FREE_AND_NULL(host_name);
    // }
}
static void network_ethernet_print_config(const eth_config_t* eth_config) {
    // #if defined(CONFIG_ETH_PHY_INTERFACE_RMII)
    //     if(eth_config->)
    //     ESP_LOGI(TAG,
    //              "Model: %s, rst=%d, mdc=%d, mdio=%d, host=%d, cs=%d, mosi=%d, miso=%d, intr=%d, clk=%d, speed=%d, tx_en=%d, tx0=%d, tx1=%d, rx0=%d, rx1=%d, crs_dv=%d",
    //              eth_config->model, eth_config->rst, eth_config->mdc, eth_config->mdio, eth_config->host, eth_config->cs,
    //              eth_config->mosi, eth_config->miso, eth_config->intr, eth_config->clk, eth_config->speed,
    //              eth_config->tx_en, eth_config->tx0, eth_config->tx1, eth_config->rx0, eth_config->rx1, eth_config->crs_dv);
    // #else
    //     ESP_LOGI(TAG, "Model: %s, rst=%d, mdc=%d, mdio=%d, host=%d, cs=%d, mosi=%d, miso=%d, intr=%d, clk=%d, speed=%d ",
    //              eth_config->model, eth_config->rst, eth_config->mdc, eth_config->mdio, eth_config->host, eth_config->cs,
    //              eth_config->mosi, eth_config->miso, eth_config->intr, eth_config->clk, eth_config->speed);
    //         :
    // #endif
}

void init_network_ethernet() {
    esp_err_t err = ESP_OK;
    esp_eth_mac_t* mac;
    esp_eth_phy_t* phy;
    eth_config_t eth;
    config_eth_init(&eth);
    ESP_LOGD(TAG, "Attempting to initialize Ethernet");
    // quick check if we have a valid ethernet configuration
    if (!eth.valid) {
        ESP_LOGI(TAG, "No ethernet");
        return;
    }
    network_driver = network_ethernet_driver_autodetect(eth.model);
    if (!network_driver) {
        messaging_post_message(MESSAGING_ERROR, MESSAGING_CLASS_SYSTEM, "Invalid ethernet Ethernet chip %s", eth.model);
        return;
    }
    if (!network_driver->valid) {
        messaging_post_message(MESSAGING_ERROR, MESSAGING_CLASS_SYSTEM, "Code not compiled for Ethernet chip %s", eth.model);
        return;
    }
    network_driver->init_config(&eth);
    network_ethernet_print_config(&eth);
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    eth_netif = esp_netif_new(&cfg);
    esp_eth_set_default_handlers(eth_netif);
    esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL);
    ethernet_event_group = xEventGroupCreate();
	xEventGroupClearBits(ethernet_event_group, LINK_UP_BIT);
    spi_device_handle_t spi_handle = NULL;
    if (network_driver->eth_config.spi) {
        spi_host_device_t host = SPI3_HOST;

        if (eth.host != -1) {
            // don't use system's shared SPI
            spi_bus_config_t buscfg = {
                .miso_io_num = eth.miso,
                .mosi_io_num = eth.mosi,
                .sclk_io_num = eth.clk,
                .quadwp_io_num = -1,
                .quadhd_io_num = -1,
            };

            // can't use SPI0
            if (eth.host == 1)
                host = SPI2_HOST;
            ESP_LOGI(TAG, "Initializing SPI bus on host %d with mosi %d and miso %d", host, eth.mosi, eth.miso);
            err = spi_bus_initialize(host, &buscfg, 1);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "SPI bus init failed : %s", esp_err_to_name(err));
            }

        } else {
            // when we use shared SPI, we assume it has been initialized
            host = spi_system_host;
        }
        if (err == ESP_OK) {
            spi_device_interface_config_t devcfg = {
                .command_bits = 1,
                .address_bits = 7,
                .mode = 0,
                .clock_speed_hz = eth.speed,
                .spics_io_num = eth.cs,
                .queue_size = 20};
            ESP_LOGI(TAG, "Adding ethernet SPI on host %d with mosi %d and miso %d", host, eth.mosi, eth.miso);
            err = spi_bus_add_device(host, &devcfg, &spi_handle);
        }
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "SPI host failed : %s", esp_err_to_name(err));
        }
    }
    if (err == ESP_OK) {
        ESP_LOGD(TAG, "Setting up ethernet driver");
        mac = network_driver->mac_new(spi_handle, &eth);
        phy = network_driver->phy_new(&eth);
        esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
        err = esp_eth_driver_install(&config, &eth_handle);
    }
    if (err == ESP_OK) {
        ESP_LOGD(TAG, "Attaching ethernet to network interface");
        err = esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle));
    }
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Starting ethernet network");
        err = esp_eth_start(eth_handle);
    }
    if (err != ESP_OK) {
        messaging_post_message(MESSAGING_ERROR, MESSAGING_CLASS_SYSTEM, "Configuring Ethernet failed: %s", esp_err_to_name(err));
        eth_handle = NULL;
    }
}

void network_ethernet_start_timer() {
    ETH_timer = xTimerCreate("ETH check", pdMS_TO_TICKS(ETH_TIMEOUT_MS), pdFALSE, NULL, ETH_Timeout);
}

/** Event handler for Ethernet events */
static void eth_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    uint8_t mac_addr[6] = {0};
    /* we can get the ethernet driver handle from event data */
    if (event_base == ETH_EVENT) {
        esp_eth_handle_t eth_handle = *(esp_eth_handle_t*)event_data;
        switch (event_id) {
            case ETHERNET_EVENT_CONNECTED:
                xEventGroupSetBits(ethernet_event_group, LINK_UP_BIT);
                esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
                ESP_LOGI(TAG, "");
                ESP_LOGI(TAG, "Ethernet Link Up, HW Addr %02x:%02x:%02x:%02x:%02x:%02x", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
                ESP_LOGD(TAG, "Sending EVENT_ETH_LINK_UP message to network manager");
                network_manager_async_link_up();
                break;
            case ETHERNET_EVENT_DISCONNECTED:
                ESP_LOGI(TAG, "Ethernet Link Down");
                xEventGroupClearBits(ethernet_event_group, LINK_UP_BIT);
                ESP_LOGD(TAG, "Sending EVENT_ETH_LINK_DOWN message to network manager");
                network_manager_async_link_down();
                break;
            case ETHERNET_EVENT_START:
                ESP_LOGI(TAG, "Ethernet Started. Setting host name");
                set_host_name();
                break;
            case ETHERNET_EVENT_STOP:
                ESP_LOGI(TAG, "Ethernet Stopped");
                break;
            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_ETH_GOT_IP) {
        network_manager_ethernet_ip_event_handler(arg, event_base, event_id, event_data);
    }
}

static void ETH_Timeout(void* timer_id) {
    network_manager_async_fail();
}

static void network_manager_ethernet_ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base != IP_EVENT)
        return;
    ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
    ip_event_got_ip_t* s = event_data;
    tcpip_adapter_if_t index = s->if_index;
    esp_netif_ip_info_t* ip_info = &s->ip_info;

    ESP_LOGI(TAG, "Got an IP address from Ethernet interface #%i. IP=" IPSTR ", Gateway=" IPSTR ", NetMask=" IPSTR ", %s",
             index,
             IP2STR(&ip_info->ip),
             IP2STR(&ip_info->gw),
             IP2STR(&ip_info->netmask),
             s->ip_changed ? "Address was changed" : "Address unchanged");
    ip_event_got_ip_t* parm = malloc(sizeof(ip_event_got_ip_t));
    memcpy(parm, event_data, sizeof(ip_event_got_ip_t));
    network_manager_async_got_ip(parm);
}