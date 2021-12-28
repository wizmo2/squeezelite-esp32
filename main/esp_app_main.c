/* 
 *  Squeezelite for esp32
 *
 *  (c) Sebastien 2019
 *      Philippe G. 2019, philippe_44@outlook.com
 *
 *  This software is released under the MIT License.
 *  https://opensource.org/licenses/MIT
 *
 */

#include "platform_esp32.h"
#include "led.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_wifi.h"
#include <esp_event.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/event_groups.h"
#include "mdns.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "nvs_utilities.h"
#include "trace.h"
#include "network_manager.h"
#include "squeezelite-ota.h"
#include <math.h>
#include "audio_controls.h"
#include "platform_config.h"
#include "telnet.h"
#include "messaging.h"
#include "gds.h"
#include "gds_default_if.h"
#include "gds_draw.h"
#include "gds_text.h"
#include "gds_font.h"
#include "display.h"
#include "accessors.h"
#include "cmd_system.h"
#include "tools.h"

static const char certs_namespace[] = "certificates";
static const char certs_key[] = "blob";
static const char certs_version[] = "version";
const char unknown_string_placeholder[] = "unknown";
const char null_string_placeholder[] = "null";
EventGroupHandle_t network_event_group;

bool bypass_network_manager=false;
const int CONNECTED_BIT = BIT0;
#define JOIN_TIMEOUT_MS (10000)
#define LOCAL_MAC_SIZE 20
static const char TAG[] = "esp_app_main";
#define DEFAULT_HOST_NAME "squeezelite"
char * fwurl = NULL;
RTC_NOINIT_ATTR uint32_t RebootCounter ;
RTC_NOINIT_ATTR uint32_t RecoveryRebootCounter ;

static bool bNetworkConnected=false;
extern const uint8_t server_cert_pem_start[] asm("_binary_github_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_github_pem_end");

// as an exception _init function don't need include
extern void services_init(void);
extern void	display_init(char *welcome);
const char * str_or_unknown(const char * str) { return (str?str:unknown_string_placeholder); }
const char * str_or_null(const char * str) { return (str?str:null_string_placeholder); }
bool is_recovery_running;

void cb_connection_got_ip(nm_state_t new_state, int sub_state){
	const char *hostname;
	static ip4_addr_t ip;
	tcpip_adapter_ip_info_t ipInfo; 
	network_get_ip_info(&ipInfo);
	if (ip.addr && ipInfo.ip.addr != ip.addr) {
		ESP_LOGW(TAG, "IP change, need to reboot");
		if(!wait_for_commit()){
			ESP_LOGW(TAG,"Unable to commit configuration. ");
		}
		esp_restart();
	}

	// initializing mDNS
	network_get_hostname(&hostname);
    mdns_init();
    mdns_hostname_set(hostname);
	
	ESP_LOGI(TAG, "Network connected and mDNS initialized with %s", hostname);

	messaging_post_message(MESSAGING_INFO,MESSAGING_CLASS_SYSTEM,"Network connected");
	xEventGroupSetBits(network_event_group, CONNECTED_BIT);
	bNetworkConnected=true;

	led_unpush(LED_GREEN);
		if(is_recovery_running){
		// when running in recovery, send a LMS discovery message 
		// to find a running instance. This is to enable using 
		// the plugin's proxy mode for FW download and avoid
		// expired certificate issues.
		discover_ota_server(5);
	}
}
void cb_connection_sta_disconnected(nm_state_t new_state, int sub_state){
	led_blink_pushed(LED_GREEN, 250, 250);
	messaging_post_message(MESSAGING_WARNING,MESSAGING_CLASS_SYSTEM,"Wifi disconnected");
	bNetworkConnected=false;
	xEventGroupClearBits(network_event_group, CONNECTED_BIT);
}
bool wait_for_wifi(){
	bool connected=(xEventGroupGetBits(network_event_group) & CONNECTED_BIT)!=0;
	if(!connected){
		ESP_LOGD(TAG,"Waiting for Network...");
	    connected = (xEventGroupWaitBits(network_event_group, CONNECTED_BIT,
	                                   pdFALSE, pdTRUE, JOIN_TIMEOUT_MS / portTICK_PERIOD_MS)& CONNECTED_BIT)!=0;
	    if(!connected){
	    	ESP_LOGW(TAG,"Network timeout.");
	    }
	    else
	    {
	    	ESP_LOGI(TAG,"Network Connected!");
	    }
	}
    return connected;
}

char * process_ota_url(){
    ESP_LOGI(TAG,"Checking for update url");
    char * fwurl=config_alloc_get(NVS_TYPE_STR, "fwurl");
	if(fwurl!=NULL)
	{
		ESP_LOGD(TAG,"Deleting nvs entry for Firmware URL %s", fwurl);
		config_delete_key("fwurl");
	}
	return fwurl;
}

esp_log_level_t  get_log_level_from_char(char * level){
	if(!strcasecmp(level, "NONE"    )) { return ESP_LOG_NONE  ;}
	if(!strcasecmp(level, "ERROR"   )) { return ESP_LOG_ERROR ;}
	if(!strcasecmp(level, "WARN"    )) { return ESP_LOG_WARN  ;}
	if(!strcasecmp(level, "INFO"    )) { return ESP_LOG_INFO  ;}
	if(!strcasecmp(level, "DEBUG"   )) { return ESP_LOG_DEBUG ;}
	if(!strcasecmp(level, "VERBOSE" )) { return ESP_LOG_VERBOSE;}
	return ESP_LOG_WARN;
}
void set_log_level(char * tag, char * level){
	esp_log_level_set(tag, get_log_level_from_char(level));
}


esp_err_t update_certificates(bool force){

	nvs_handle handle;
	esp_err_t esp_err;
    esp_app_desc_t running_app_info;

	ESP_LOGI(TAG,   "About to check if certificates need to be updated in flash");
	esp_err = nvs_open_from_partition(settings_partition, certs_namespace, NVS_READWRITE, &handle);
	if (esp_err != ESP_OK) {
		LOG_SEND(MESSAGING_INFO,"Unable to update HTTPS certificates. Could not open NVS namespace %s. Error %s", certs_namespace, esp_err_to_name(esp_err));
		return esp_err;
	}

	const esp_partition_t *running = esp_ota_get_running_partition();
	ESP_LOGI(TAG, "Running partition [%s] type %d subtype %d (offset 0x%08x)", running->label, running->type, running->subtype, running->address);

	if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
		ESP_LOGI(TAG, "Running version: %s", running_app_info.version);
	}

	size_t len=0;
	char *str=NULL;
	bool changed=false;
	if ( (esp_err= nvs_get_str(handle, certs_version, NULL, &len)) == ESP_OK) {
		str=(char *)malloc_init_external(len+1);
		if(str){
			if ( (esp_err = nvs_get_str(handle,  certs_version, str, &len)) == ESP_OK) {
				ESP_LOGI(TAG,"Certificate version: %s", str);
			}
		}
	}
	if(str!=NULL && running->subtype !=ESP_PARTITION_SUBTYPE_APP_FACTORY){
		// If certificates were found in nvs, only update if we're not
		// running recovery. This will prevent rolling back to an older version
		if(strcmp((char *)running_app_info.version,(char *)str )){
			// Versions are different
			ESP_LOGW(TAG,"Found a different software version. Updating certificates");
			changed=true;
		}
		free(str);
	}
	else if(str==NULL){
		ESP_LOGW(TAG,"No certificate found. Adding certificates");
		changed=true;
	}

	if(changed || force){

		esp_err = nvs_set_blob(handle, certs_key, server_cert_pem_start, (server_cert_pem_end-server_cert_pem_start));
		if(esp_err!=ESP_OK){
			log_send_messaging(MESSAGING_ERROR,"Failed to store certificate data: %s", esp_err_to_name(esp_err));
		}
		else {
			esp_err = nvs_set_str(handle,  certs_version, running_app_info.version);
			if(esp_err!=ESP_OK){
				log_send_messaging(MESSAGING_ERROR,"Unable to update HTTPS Certificates version: %s",esp_err_to_name(esp_err));
			}
			else {
				esp_err = nvs_commit(handle);
				if(esp_err!=ESP_OK){
					log_send_messaging(MESSAGING_ERROR,"Failed to commit certificates changes : %s",esp_err_to_name(esp_err));
				}
				else {
					log_send_messaging(MESSAGING_INFO,"HTTPS Certificates were updated with version: %s",running_app_info.version);
				}
			}
		}
	}

	nvs_close(handle);
	return ESP_OK;
}
const char * get_certificate(){
	nvs_handle handle;
	esp_err_t esp_err;
	char *blob =NULL;
//
	ESP_LOGD(TAG,  "Fetching certificate.");
	esp_err = nvs_open_from_partition(settings_partition, certs_namespace, NVS_READONLY, &handle);
	if(esp_err == ESP_OK){
        size_t len;
        esp_err = nvs_get_blob(handle, certs_key, NULL, &len);
        if( esp_err == ESP_OK) {
			blob = (char *) malloc_init_external(len+1);
            if(!blob){
            	log_send_messaging(MESSAGING_ERROR,"Unable to retrieve HTTPS certificates. %s","Memory allocation failed");
        		return "";
            }
            memset(blob,0x00,len+1);
            esp_err = nvs_get_blob(handle, certs_key, blob, &len);
            if ( esp_err  == ESP_OK) {
            	ESP_LOGD(TAG,"Certificates content is %d bytes long: ", len);
            }
            else {
            	log_send_messaging(MESSAGING_ERROR,"Unable to retrieve HTTPS certificates. Get blob failed: %s", esp_err_to_name(esp_err));
            }
        }
        else{
        	log_send_messaging(MESSAGING_ERROR,"Unable to retrieve HTTPS certificates. Get blob failed: %s",esp_err_to_name(esp_err));
        }
        nvs_close(handle);
	}
	else{
    	log_send_messaging(MESSAGING_ERROR,"Unable to retrieve HTTPS certificates. NVS name space %s open failed: %s",certs_namespace, esp_err_to_name(esp_err));
	}
	return blob;
}

#define DEFAULT_NAME_WITH_MAC(var,defval) char var[strlen(defval)+sizeof(macStr)]; strcpy(var,defval); strcat(var,macStr)
void register_default_string_val(const char * key, char * value){
	char * existing =(char *)config_alloc_get(NVS_TYPE_STR,key );
	ESP_LOGD(TAG,"Register default called with:  %s= %s",key,value );
	if(!existing) {
		ESP_LOGI(TAG,"Registering default value for key %s, value %s",key,value );
		config_set_default(NVS_TYPE_STR, key,value, 0);
	}
	else {
		ESP_LOGD(TAG,"Value found for %s: %s",key,existing );
	}
	FREE_AND_NULL(existing);
}

void register_default_with_mac(const char* key,  char* defval) {
    uint8_t mac[6];
    char macStr[LOCAL_MAC_SIZE + 1];
    char* fullvalue = NULL;
    esp_read_mac((uint8_t*)&mac, ESP_MAC_WIFI_STA);
    snprintf(macStr, LOCAL_MAC_SIZE - 1, "-%x%x%x", mac[3], mac[4], mac[5]);
	fullvalue = malloc_init_external(strlen(defval)+sizeof(macStr)+1);
	if(fullvalue){
		strcpy(fullvalue, defval);
		strcat(fullvalue, macStr);
		register_default_string_val(key,fullvalue);
		FREE_AND_NULL(fullvalue);
	}
	else {
		ESP_LOGE(TAG,"Memory allocation failed when registering default value for %s", key);
	}
	
}

void register_default_nvs(){
#ifdef CONFIG_CSPOT_SINK
	register_default_string_val("enable_cspot", STR(CONFIG_CSPOT_SINK));
#endif
	
#ifdef CONFIG_AIRPLAY_SINK
	register_default_with_mac("airplay_name", CONFIG_AIRPLAY_NAME);
	register_default_string_val("airplay_port", CONFIG_AIRPLAY_PORT);
	register_default_string_val( "enable_airplay", STR(CONFIG_AIRPLAY_SINK));
#endif

#ifdef CONFIG_BT_SINK
	register_default_string_val( "bt_sink_pin", STR(CONFIG_BT_SINK_PIN));
	register_default_string_val( "bt_sink_volume", "127");
	register_default_with_mac("bt_name", CONFIG_BT_NAME);
	register_default_string_val( "enable_bt_sink", STR(CONFIG_BT_SINK));
	register_default_string_val("a2dp_dev_name", CONFIG_A2DP_DEV_NAME);
	register_default_string_val("a2dp_ctmt", STR(CONFIG_A2DP_CONNECT_TIMEOUT_MS));
	register_default_string_val("a2dp_ctrld", STR(CONFIG_A2DP_CONTROL_DELAY_MS));
	register_default_string_val("a2dp_sink_name", CONFIG_A2DP_SINK_NAME);
#endif
	
	register_default_with_mac("host_name", DEFAULT_HOST_NAME);
	register_default_with_mac("ap_ssid", CONFIG_DEFAULT_AP_SSID);
	register_default_string_val("autoexec","1");
	register_default_with_mac("autoexec1",CONFIG_DEFAULT_COMMAND_LINE " -n " DEFAULT_HOST_NAME);	

	register_default_string_val("release_url", CONFIG_SQUEEZELITE_ESP32_RELEASE_URL);
	register_default_string_val("ap_ip_address",CONFIG_DEFAULT_AP_IP);
	register_default_string_val("ap_ip_gateway",CONFIG_DEFAULT_AP_GATEWAY );
	register_default_string_val("ap_ip_netmask",CONFIG_DEFAULT_AP_NETMASK);
	register_default_string_val("ap_channel",STR(CONFIG_DEFAULT_AP_CHANNEL));
	register_default_string_val("ap_pwd", CONFIG_DEFAULT_AP_PASSWORD);
	register_default_string_val("bypass_wm", "0");
	register_default_string_val("equalizer", "");
	register_default_string_val("actrls_config", "");	
	register_default_string_val("lms_ctrls_raw", "n");	
	register_default_string_val("rotary_config", CONFIG_ROTARY_ENCODER);
	char number_buffer[101] = {};
	snprintf(number_buffer,sizeof(number_buffer)-1,"%u",OTA_FLASH_ERASE_BLOCK);
	register_default_string_val( "ota_erase_blk", number_buffer);
	snprintf(number_buffer,sizeof(number_buffer)-1,"%u",OTA_STACK_SIZE);
	register_default_string_val( "ota_stack", number_buffer);
	snprintf(number_buffer,sizeof(number_buffer)-1,"%d",OTA_TASK_PRIOTITY);
	register_default_string_val( "ota_prio", number_buffer);
	register_default_string_val( "display_config", CONFIG_DISPLAY_CONFIG);
	register_default_string_val( "eth_config", CONFIG_ETH_CONFIG);
	register_default_string_val( "i2c_config", CONFIG_I2C_CONFIG);
	register_default_string_val( "spi_config", CONFIG_SPI_CONFIG);
	register_default_string_val( "set_GPIO", CONFIG_SET_GPIO);
	register_default_string_val( "led_brightness", "");
	register_default_string_val( "spdif_config", "");
	register_default_string_val( "dac_config", "");
	register_default_string_val( "dac_controlset", "");
	register_default_string_val( "jack_mutes_amp", "n");
	register_default_string_val("gpio_exp_config", CONFIG_GPIO_EXP_CONFIG);
	register_default_string_val( "bat_config", "");
	register_default_string_val( "metadata_config", "");
	register_default_string_val( "telnet_enable", "");
	register_default_string_val( "telnet_buffer", "40000");
	register_default_string_val( "telnet_block", "500");
	register_default_string_val( "stats", "n");
	register_default_string_val( "rel_api", CONFIG_RELEASE_API);
	register_default_string_val("pollmx","600");
    register_default_string_val("pollmin","15");
    register_default_string_val("ethtmout","8");
    register_default_string_val("dhcp_tmout","8");
	register_default_string_val("wifi_smode", "A");
	wait_for_commit();
	ESP_LOGD(TAG,"Done setting default values in nvs.");
}

uint32_t halSTORAGE_RebootCounterRead(void) { return RebootCounter ; }
uint32_t halSTORAGE_RebootCounterUpdate(int32_t xValue) { 
	if(RebootCounter >100) {
		RebootCounter = 0;
		RecoveryRebootCounter = 0;
	}
	RebootCounter = (xValue != 0) ? (RebootCounter + xValue) : 0;
	RecoveryRebootCounter  = (xValue != 0) && is_recovery_running ? (RecoveryRebootCounter + xValue) : 0;
	return (RebootCounter) ; 
	}

void handle_ap_connect(nm_state_t new_state, int sub_state){
	start_telnet(NULL);
}
void handle_network_up(nm_state_t new_state, int sub_state){
	halSTORAGE_RebootCounterUpdate(0);
}
esp_reset_reason_t xReason=ESP_RST_UNKNOWN;

void app_main()
{
	const esp_partition_t *running = esp_ota_get_running_partition();
	is_recovery_running = (running->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY);
	xReason = esp_reset_reason();
	ESP_LOGI(TAG,"Reset reason is: %u", xReason);
	if(!is_recovery_running )  {
		/* unscheduled restart (HW, Watchdog or similar) thus increment dynamic
	 	* counter then log current boot statistics as a warning */
		uint32_t Counter = halSTORAGE_RebootCounterUpdate(1) ;		// increment counter
		ESP_LOGI(TAG,"Reboot counter=%u\n", Counter) ;
		if (Counter == 5) {
			guided_factory();
		}
	}
	else {
		uint32_t Counter = halSTORAGE_RebootCounterUpdate(1) ;		// increment counter
		if(RecoveryRebootCounter==1 && Counter>=5){
			// First time we are rebooting in recovery after crashing
			messaging_post_message(MESSAGING_ERROR,MESSAGING_CLASS_SYSTEM,"System was forced into recovery mode after crash likely caused by some bad configuration\n");
		}
		ESP_LOGI(TAG,"Recovery Reboot counter=%u\n", Counter) ;
			if (RecoveryRebootCounter == 5) {
			ESP_LOGW(TAG,"System rebooted too many times. This could be an indication that configuration is corrupted. Erasing config.");
			erase_settings_partition();
			// reboot one more time
			guided_factory();
			
		}		
		if (RecoveryRebootCounter >5){
			messaging_post_message(MESSAGING_ERROR,MESSAGING_CLASS_SYSTEM,"System was forced into recovery mode after crash likely caused by some bad configuration. Configuration was reset to factory.\n");
		}	
	}

	char * fwurl = NULL;
	MEMTRACE_PRINT_DELTA();
	ESP_LOGI(TAG,"Starting app_main");
	initialize_nvs();
	MEMTRACE_PRINT_DELTA();
	ESP_LOGI(TAG,"Setting up telnet.");
	init_telnet(); // align on 32 bits boundaries
	MEMTRACE_PRINT_DELTA();
	ESP_LOGI(TAG,"Setting up config subsystem.");
	config_init();
	MEMTRACE_PRINT_DELTA();
	ESP_LOGD(TAG,"Creating event group for wifi");
	network_event_group = xEventGroupCreate();
	ESP_LOGD(TAG,"Clearing CONNECTED_BIT from wifi group");
	xEventGroupClearBits(network_event_group, CONNECTED_BIT);

	ESP_LOGI(TAG,"Registering default values");
	register_default_nvs();
	MEMTRACE_PRINT_DELTA();
	ESP_LOGI(TAG,"Configuring services");
	services_init();
	MEMTRACE_PRINT_DELTA();
	ESP_LOGI(TAG,"Initializing display");
	display_init("SqueezeESP32");
	MEMTRACE_PRINT_DELTA();
	if(is_recovery_running && display){
		GDS_ClearExt(display, true);
		GDS_SetFont(display, &Font_line_2 );
		GDS_TextPos(display, GDS_FONT_DEFAULT, GDS_TEXT_CENTERED, GDS_TEXT_CLEAR | GDS_TEXT_UPDATE, "RECOVERY");
	}


	ESP_LOGI(TAG,"Checking if certificates need to be updated");
	update_certificates(false);
	MEMTRACE_PRINT_DELTA();

	ESP_LOGD(TAG,"Getting firmware OTA URL (if any)");
	fwurl = process_ota_url();

	ESP_LOGD(TAG,"Getting value for WM bypass, nvs 'bypass_wm'");
	char * bypass_wm = config_alloc_get_default(NVS_TYPE_STR, "bypass_wm", "0", 0);
	if(bypass_wm==NULL)
	{
		ESP_LOGE(TAG, "Unable to retrieve the Wifi Manager bypass flag");
		bypass_network_manager = false;
	}
	else {
		bypass_network_manager=(strcmp(bypass_wm,"1")==0 ||strcasecmp(bypass_wm,"y")==0);
	}

	if(!is_recovery_running){
		ESP_LOGD(TAG,"Getting audio control mapping ");
		char *actrls_config = config_alloc_get_default(NVS_TYPE_STR, "actrls_config", NULL, 0);
		if (actrls_init(actrls_config) == ESP_OK) {
			ESP_LOGD(TAG,"Initializing audio control buttons type %s", actrls_config);	
		} else {
			ESP_LOGD(TAG,"No audio control buttons");
		}
		if (actrls_config) free(actrls_config);
	}

	/* start the wifi manager */
	ESP_LOGD(TAG,"Blinking led");
	led_blink_pushed(LED_GREEN, 250, 250);
	MEMTRACE_PRINT_DELTA();
	if(bypass_network_manager){
		ESP_LOGW(TAG,"Network manager is disabled. Use command line for wifi control.");
	}
	else {
		ESP_LOGI(TAG,"Starting Network Manager");
		network_start();
		MEMTRACE_PRINT_DELTA();
		network_register_state_callback(NETWORK_WIFI_ACTIVE_STATE,WIFI_CONNECTED_STATE, "cb_connection_got_ip", &cb_connection_got_ip);
		network_register_state_callback(NETWORK_ETH_ACTIVE_STATE,ETH_ACTIVE_CONNECTED_STATE, "cb_connection_got_ip",&cb_connection_got_ip);
		network_register_state_callback(NETWORK_WIFI_ACTIVE_STATE,WIFI_LOST_CONNECTION_STATE, "cb_connection_sta_disconnected",&cb_connection_sta_disconnected);

		/* Start the telnet service after we are certain that the network stack has been properly initialized.
		 * This can be either after we're started the AP mode, or after we've started the STA mode  */
		network_register_state_callback(NETWORK_INITIALIZING_STATE,-1, "handle_ap_connect", &handle_ap_connect);
		network_register_state_callback(NETWORK_ETH_ACTIVE_STATE,ETH_ACTIVE_LINKDOWN_STATE, "handle_network_up", &handle_network_up);
		network_register_state_callback(NETWORK_WIFI_ACTIVE_STATE,WIFI_INITIALIZING_STATE, "handle_network_up", &handle_network_up);
		MEMTRACE_PRINT_DELTA();
		
	}
	MEMTRACE_PRINT_DELTA_MESSAGE("Starting Console");
	console_start();
	MEMTRACE_PRINT_DELTA_MESSAGE("Console started");
	if(fwurl && strlen(fwurl)>0){
		if(is_recovery_running){
			while(!bNetworkConnected){
				wait_for_wifi();
				taskYIELD();
			}
			ESP_LOGI(TAG,"Updating firmware from link: %s",fwurl);
			start_ota(fwurl, NULL, 0);
		}
		else {
			ESP_LOGE(TAG,"Restarted to application partition. We're not going to perform OTA!");
		}
		free(fwurl);
	}
	messaging_post_message(MESSAGING_INFO,MESSAGING_CLASS_SYSTEM,"System started");
}
