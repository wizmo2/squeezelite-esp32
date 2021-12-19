/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "platform_config.h"
#include "gpio_exp.h"
#include "led.h"
#include "globdefs.h"
#include "accessors.h"

#define MAX_LED	8
#define BLOCKTIME	10	// up to portMAX_DELAY

static const char *TAG = "led";

static EXT_RAM_ATTR struct led_s {
	gpio_num_t gpio;
	bool on;
	int onstate;
	int ontime, offtime;
	int pwm;
	int channel;
	int pushedon, pushedoff;
	bool pushed;
	TimerHandle_t timer;
} leds[MAX_LED];

// can't use EXT_RAM_ATTR for initialized structure
static struct {
	int gpio;
	int active;
	int pwm;
} green = { .gpio = CONFIG_LED_GREEN_GPIO, .active = 0, .pwm = -1 },
  red = { .gpio = CONFIG_LED_RED_GPIO, .active = 0, .pwm = -1 };
  
static int led_max = 2;

/****************************************************************************************
 * 
 */
static void set_level(struct led_s *led, bool on) {
	if (led->pwm < 0 || led->gpio >= GPIO_NUM_MAX) gpio_set_level_x(led->gpio, on ? led->onstate : !led->onstate);
	else {
		ledc_set_duty(LEDC_HIGH_SPEED_MODE, led->channel, on ? led->pwm : (led->onstate ? 0 : pwm_system.max));
		ledc_update_duty(LEDC_HIGH_SPEED_MODE, led->channel);
	}		
}

/****************************************************************************************
 * 
 */
static void vCallbackFunction( TimerHandle_t xTimer ) {
	struct led_s *led = (struct led_s*) pvTimerGetTimerID (xTimer);
	
	if (!led->timer) return;
	
	led->on = !led->on;
	ESP_EARLY_LOGD(TAG,"led vCallbackFunction setting gpio %d level %d (pwm:%d)", led->gpio, led->on, led->pwm);
	set_level(led, led->on);
		
	// was just on for a while
	if (!led->on && led->offtime == -1) return;
	
	// regular blinking
	xTimerChangePeriod(xTimer, (led->on ? led->ontime : led->offtime) / portTICK_RATE_MS, BLOCKTIME);
}

/****************************************************************************************
 * 
 */
bool led_blink_core(int idx, int ontime, int offtime, bool pushed) {
	if (!leds[idx].gpio || leds[idx].gpio < 0 ) return false;
	
	ESP_LOGD(TAG,"led_blink_core %d on:%d off:%d, pushed:%u", idx, ontime, offtime, pushed);
	if (leds[idx].timer) {
		// normal requests waits if a pop is pending
		if (!pushed && leds[idx].pushed) {
			leds[idx].pushedon = ontime; 
			leds[idx].pushedoff = offtime; 
			return true;
		}
		xTimerStop(leds[idx].timer, BLOCKTIME);
	}
	
	// save current state if not already pushed
	if (!leds[idx].pushed) {
		leds[idx].pushedon = leds[idx].ontime;
		leds[idx].pushedoff = leds[idx].offtime;	
		leds[idx].pushed = pushed;
	}	
	
	// then set new one
	leds[idx].ontime = ontime;
	leds[idx].offtime = offtime;	
			
	if (ontime == 0) {
		ESP_LOGD(TAG,"led %d, setting reverse level", idx);
		set_level(leds + idx, false);
	} else if (offtime == 0) {
		ESP_LOGD(TAG,"led %d, setting level", idx);
		set_level(leds + idx, true);
	} else {
		if (!leds[idx].timer) {
			ESP_LOGD(TAG,"led %d, Creating timer", idx);
			leds[idx].timer = xTimerCreate("ledTimer", ontime / portTICK_RATE_MS, pdFALSE, (void *)&leds[idx], vCallbackFunction);
		}
        leds[idx].on = true;
		set_level(leds + idx, true);

        ESP_LOGD(TAG,"led %d, Setting gpio %d and starting timer", idx, leds[idx].gpio);
		if (xTimerStart(leds[idx].timer, BLOCKTIME) == pdFAIL) return false;
	}
	
	
	return true;
} 

/****************************************************************************************
 * 
 */
bool led_brightness(int idx, int pwm) {
	if (pwm > 100) pwm = 100;
	leds[idx].pwm = pwm_system.max * powf(pwm / 100.0, 3);
	if (!leds[idx].onstate) leds[idx].pwm = pwm_system.max - leds[idx].pwm;
	
	ledc_set_duty(LEDC_HIGH_SPEED_MODE, leds[idx].channel, leds[idx].pwm);
	ledc_update_duty(LEDC_HIGH_SPEED_MODE, leds[idx].channel);
	
	return true;
}

/****************************************************************************************
 * 
 */
bool led_unpush(int idx) {
	if (!leds[idx].gpio || leds[idx].gpio<0) return false;
	
	led_blink_core(idx, leds[idx].pushedon, leds[idx].pushedoff, true);
	leds[idx].pushed = false;
	
	return true;
}	

/****************************************************************************************
 * 
 */
int led_allocate(void) {
	 if (led_max < MAX_LED) return led_max++;
	 return -1;
}

/****************************************************************************************
 * 
 */
bool led_config(int idx, gpio_num_t gpio, int onstate, int pwm) {
	if (gpio < 0) {
		ESP_LOGW(TAG,"LED GPIO -1 ignored");
		return false;
	}
	
	ESP_LOGD(TAG,"Index %d, GPIO %d, on state %s", idx, gpio, onstate>0?"On":"Off");
	if (idx >= MAX_LED) return false;
	
	leds[idx].gpio = gpio;
	leds[idx].onstate = onstate;
	leds[idx].pwm = -1;

	if (pwm < 0 || gpio >= GPIO_NUM_MAX) {	
		gpio_pad_select_gpio_x(gpio);
		gpio_set_direction_x(gpio, GPIO_MODE_OUTPUT);
	} else {	
		leds[idx].channel = pwm_system.base_channel++;
		leds[idx].pwm = pwm_system.max * powf(pwm / 100.0, 3);
		if (!onstate) leds[idx].pwm = pwm_system.max - leds[idx].pwm;
		
		ledc_channel_config_t ledc_channel = {
            .channel    = leds[idx].channel,
            .duty       = leds[idx].pwm,
            .gpio_num   = gpio,
            .speed_mode = LEDC_HIGH_SPEED_MODE,
            .hpoint     = 0,
            .timer_sel  = pwm_system.timer,
        };
		
		ledc_channel_config(&ledc_channel);
	}
	
	set_level(leds + idx, false);
	ESP_LOGD(TAG,"PWM Index %d, GPIO %d, on state %s, pwm %d%%", idx, gpio, onstate > 0 ? "On" : "Off", pwm);		

	return true;
}

/****************************************************************************************
 * 
 */
void set_led_gpio(int gpio, char *value) {
	char *p;
	
	if (strcasestr(value, "green")) {
		green.gpio = gpio;
		if ((p = strchr(value, ':')) != NULL) green.active = atoi(p + 1);
	} else 	if (strcasestr(value, "red")) {
		red.gpio = gpio;
		if ((p = strchr(value, ':')) != NULL) red.active = atoi(p + 1);
	}	
}

void led_svc_init(void) {
#ifdef CONFIG_LED_GREEN_GPIO_LEVEL
	green.active = CONFIG_LED_GREEN_GPIO_LEVEL;
#endif
#ifdef CONFIG_LED_RED_GPIO_LEVEL
	red.active = CONFIG_LED_RED_GPIO_LEVEL;
#endif

#ifndef CONFIG_LED_LOCKED
	parse_set_GPIO(set_led_gpio);
#endif

	char *nvs_item = config_alloc_get(NVS_TYPE_STR, "led_brightness"); 
	if (nvs_item) {
		PARSE_PARAM(nvs_item, "green", '=', green.pwm);
		PARSE_PARAM(nvs_item, "red", '=', red.pwm);
		free(nvs_item);
	}

	led_config(LED_GREEN, green.gpio, green.active, green.pwm);
	led_config(LED_RED, red.gpio, red.active, red.pwm);
	
	ESP_LOGI(TAG,"Configuring LEDs green:%d (active:%d %d%%), red:%d (active:%d %d%%)", green.gpio, green.active, green.pwm, red.gpio, red.active, red.pwm );
}
