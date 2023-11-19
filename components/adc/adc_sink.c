#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "nvs.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_pthread.h"
#include "esp_system.h"
#include "freertos/timers.h"
#include "platform_config.h"
#include "input_i2s.h"
//#include "audio_controls.h"
#include "display.h"
#include "accessors.h"
#include "network_services.h"

static EXT_RAM_ATTR struct adc_cb_s {
	adc_cmd_vcb_t cmd;
	adc_data_cb_t data;
} adc_cbs;

static const char TAG[] = "adc_sink";

static struct adc_ctx_s *adc_i2s;
static adc_cmd_vcb_t cmd_handler_chain;

/****************************************************************************************
 * Command handler
 */
static bool cmd_handler(adc_event_t event, ...) {
    va_list args;	
	
	va_start(args, event);
	
	// handle audio event and stop if forbidden
	if (!cmd_handler_chain(event, args)) {
		va_end(args);
		return false;
	}

	// now handle events for display
	switch(event) {
	case ADC_SETUP:
		ESP_LOGI(TAG, "ADC Setup");
		displayer_control(DISPLAYER_ACTIVATE, "ADC INPUT ACTIVE", true);
        displayer_artwork(NULL);
		break;
	case ADC_PLAY:
	    ESP_LOGI(TAG, "ADC Play");
		// control it internally
		//displayer_control(DISPLAYER_TIMER_RUN);
		displayer_control(DISPLAYER_SHUTDOWN); // hand back to lms
		break;		
	case ADC_STALLED:
        ESP_LOGI(TAG, "ADC Stalled");
        adc_abort(adc_i2s);
        displayer_control(DISPLAYER_SHUTDOWN);
		break;
	
	default: 
		ESP_LOGI(TAG, "ADC Unknown: %d", event);
		break;
	}
	
	va_end(args);
	
	return true;
}

/****************************************************************************************
 * ADC sink de-initialization
 *  Called after slimproto is terminated
 */
void adc_sink_deinit(void) {
    adc_delete(adc_i2s);
	ESP_LOGI(TAG, "deinit ADC");
}	

/****************************************************************************************
 * ADC sink startup
 */
static void adc_sink_start(nm_state_t state_id, int sub_state) {
    const char *hostname;

	cmd_handler_chain = adc_cbs.cmd;
	network_get_hostname(&hostname);
	
	ESP_LOGI(TAG, "starting ADC on host %s", hostname);
    
	adc_i2s = adc_create(cmd_handler, adc_cbs.data);
}

/****************************************************************************************
 * ADC sink initialization
 */
void adc_sink_init(adc_cmd_vcb_t cmd_cb, adc_data_cb_t data_cb) {
	adc_cbs.cmd = cmd_cb;
	adc_cbs.data = data_cb;

	network_register_state_callback(NETWORK_WIFI_ACTIVE_STATE, WIFI_CONNECTED_STATE, "adc_sink_start", adc_sink_start);
	network_register_state_callback(NETWORK_ETH_ACTIVE_STATE, ETH_ACTIVE_CONNECTED_STATE, "adc_sink_start", adc_sink_start);
}

/****************************************************************************************
 * ADC forced disconnection
 */
void adc_disconnect(void) {
	ESP_LOGI(TAG, "forced disconnection");
	displayer_control(DISPLAYER_SHUTDOWN);
	adc_cmd(adc_i2s, ADC_CLOSE, NULL);
}
/****************************************************************************************
 * ADC restart
 *   Called by slimproto when SB wants to play a song
 *   For background streaming we only want to stop playing, but leave the service running
 */
void adc_restart(uint16_t sample_rate) {
	ESP_LOGI(TAG, "ADC Restart (setup)");
    if (adc_cmd(adc_i2s, ADC_SETUP, sample_rate)) {
		ESP_LOGI(TAG, "ADC Restart (play)");
		adc_cmd(adc_i2s, ADC_PLAY, NULL);
	}
	ESP_LOGI(TAG, "ADC Restart (done)");
}

