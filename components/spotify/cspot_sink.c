#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "mdns.h"
#include "nvs.h"
#include "tcpip_adapter.h"
// IDF-V4++ #include "esp_netif.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_pthread.h"
#include "esp_system.h"
#include "freertos/timers.h"
#include "platform_config.h"
#include "audio_controls.h"
#include "display.h"
#include "accessors.h"
#include "cspot_private.h"
#include "cspot_sink.h"

static EXT_RAM_ATTR struct cspot_cb_s {
	cspot_cmd_vcb_t cmd;
	cspot_data_cb_t data;
} cspot_cbs;

static const char TAG[] = "cspot";
static struct cspot_s *cspot;
static cspot_cmd_vcb_t cmd_handler_chain;

static void cspot_volume_up(bool pressed) {
	if (!pressed) return;
	cspot_cmd(cspot, CSPOT_VOLUME_UP, NULL);
	ESP_LOGI(TAG, "CSpot volume up");
}

static void cspot_volume_down(bool pressed) {
	if (!pressed) return;
	cspot_cmd(cspot, CSPOT_VOLUME_DOWN, NULL);
	ESP_LOGI(TAG, "CSpot volume down");
}

static void cspot_toggle(bool pressed) {
	if (!pressed) return;
	cspot_cmd(cspot, CSPOT_TOGGLE, NULL);
	ESP_LOGI(TAG, "CSpot play/pause");
}

static void cspot_pause(bool pressed) {
	if (!pressed) return;
	cspot_cmd(cspot, CSPOT_PAUSE, NULL);
	ESP_LOGI(TAG, "CSpot pause");
}

static void cspot_play(bool pressed) {
	if (!pressed) return;
	cspot_cmd(cspot, CSPOT_PLAY, NULL);
	ESP_LOGI(TAG, "CSpot play");
}

static void cspot_stop(bool pressed) {
	if (!pressed) return;
	cspot_cmd(cspot, CSPOT_STOP, NULL);
	ESP_LOGI(TAG, "CSpot stop");
}

static void cspot_prev(bool pressed) {
	if (!pressed) return;
	cspot_cmd(cspot, CSPOT_PREV, NULL);
	ESP_LOGI(TAG, "CSpot previous");
}

static void cspot_next(bool pressed) {
	if (!pressed) return;
	cspot_cmd(cspot, CSPOT_NEXT, NULL);
	ESP_LOGI(TAG, "CSpot next");
}

const static actrls_t controls = {
	NULL,								// power
	cspot_volume_up, cspot_volume_down,	// volume up, volume down
	cspot_toggle, cspot_play,			// toggle, play
	cspot_pause, cspot_stop,			// pause, stop
	NULL, NULL,							// rew, fwd
	cspot_prev, cspot_next,				// prev, next
	NULL, NULL, NULL, NULL, // left, right, up, down
	NULL, NULL, NULL, NULL, NULL, NULL, // pre1-6
	cspot_volume_down, cspot_volume_up, cspot_toggle// knob left, knob_right, knob push
};

/****************************************************************************************
 * Command handler
 */
static bool cmd_handler(cspot_event_t event, ...) {
	va_list args;	
	
	va_start(args, event);
	
	// handle audio event and stop if forbidden
	if (!cmd_handler_chain(event, args)) {
		va_end(args);
		return false;
	}

	// now handle events for display
	switch(event) {
	case CSPOT_SETUP:
		actrls_set(controls, false, NULL, actrls_ir_action);
		displayer_control(DISPLAYER_ACTIVATE, "SPOTIFY");
		break;
	case CSPOT_PLAY:
		displayer_control(DISPLAYER_TIMER_RUN);
		break;		
	case CSPOT_PAUSE:
		displayer_control(DISPLAYER_TIMER_PAUSE);
		break;		
	case CSPOT_DISC:
		actrls_unset();
		displayer_control(DISPLAYER_SUSPEND);
		break;
	case CSPOT_SEEK:
		displayer_timer(DISPLAYER_ELAPSED, va_arg(args, int), -1);
		break;
	case CSPOT_TRACK: {
		uint32_t sample_rate = va_arg(args, uint32_t);
		char *artist = va_arg(args, char*), *album = va_arg(args, char*), *title = va_arg(args, char*);
		displayer_metadata(artist, album, title);
		displayer_timer(DISPLAYER_ELAPSED, 0, -1);
		break;
	}	
	// nothing to do on CSPOT_FLUSH
	default: 
		break;
	}
	
	va_end(args);
	
	return true;
}

/****************************************************************************************
 * CSpot sink de-initialization
 */
void cspot_sink_deinit(void) {
	mdns_free();
}	

/****************************************************************************************
 * CSpot sink startup
 */
static bool cspot_sink_start(cspot_cmd_vcb_t cmd_cb, cspot_data_cb_t data_cb) {
    const char *hostname = NULL;
	tcpip_adapter_ip_info_t ipInfo = { }; 
	tcpip_adapter_if_t ifs[] = { TCPIP_ADAPTER_IF_ETH, TCPIP_ADAPTER_IF_STA, TCPIP_ADAPTER_IF_AP };
   	
	// get various IP info
	for (int i = 0; i < sizeof(ifs) / sizeof(tcpip_adapter_if_t); i++) 
		if (tcpip_adapter_get_ip_info(ifs[i], &ipInfo) == ESP_OK && ipInfo.ip.addr != IPADDR_ANY) {
			tcpip_adapter_get_hostname(ifs[i], &hostname);			
			break;
		}
	
	if (!hostname) {
		ESP_LOGI(TAG,  "No hostname/IP found, can't start CSpot (will retry)");
		return false;
	}
	
	cmd_handler_chain = cmd_cb;
	cspot = cspot_create(hostname, cmd_handler, data_cb);
	
	return true;
}

/****************************************************************************************
 * CSpot sink timer handler
 */
static void cspot_start_handler( TimerHandle_t xTimer ) {
	if (cspot_sink_start(cspot_cbs.cmd, cspot_cbs.data)) {
		xTimerDelete(xTimer, portMAX_DELAY);
	}	
}	

/****************************************************************************************
 * CSpot sink initialization
 */
void cspot_sink_init(cspot_cmd_vcb_t cmd_cb, cspot_data_cb_t data_cb) {
	if (!cspot_sink_start(cmd_cb, data_cb)) {
		cspot_cbs.cmd = cmd_cb;
		cspot_cbs.data = data_cb;
		TimerHandle_t timer = xTimerCreate("cspotStart", 5000 / portTICK_RATE_MS, pdTRUE, NULL, cspot_start_handler);
		xTimerStart(timer, portMAX_DELAY);
		ESP_LOGI(TAG,  "Delaying CSPOT start");		
	}
}

/****************************************************************************************
 * CSpot forced disconnection
 */
void cspot_disconnect(void) {
	ESP_LOGI(TAG, "forced disconnection");
	displayer_control(DISPLAYER_SHUTDOWN);
	cspot_cmd(cspot, CSPOT_DISC, NULL);
	actrls_unset();
}
