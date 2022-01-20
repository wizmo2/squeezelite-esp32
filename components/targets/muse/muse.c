/*
 YOUR LICENSE
 */
#include <string.h>
#include <esp_log.h>
#include <esp_types.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
//#include <driver/adc.h>
#include "driver/rmt.h"
#include "monitor.h"

/////////////////////////////////////////////////////////////////
//*********************** NeoPixels  ***************************
////////////////////////////////////////////////////////////////
#define NUM_LEDS  1
#define LED_RMT_TX_CHANNEL   0
#define LED_RMT_TX_GPIO      22

#define BITS_PER_LED_CMD 24 
#define LED_BUFFER_ITEMS ((NUM_LEDS * BITS_PER_LED_CMD))

// These values are determined by measuring pulse timing with logic analyzer and adjusting to match datasheet. 
#define T0H 14  // 0 bit high time
#define T1H 52 // 1 bit high time
#define TL  52  // low time for either bit

#define GREEN   0xFF0000
#define RED 	0x00FF00
#define BLUE  	0x0000FF
#define WHITE   0xFFFFFF
#define YELLOW  0xE0F060
struct led_state {
    uint32_t leds[NUM_LEDS];
};

void ws2812_control_init(void);
void ws2812_write_leds(struct led_state new_state);

///////////////////////////////////////////////////////////////////

static const char TAG[] = "muse";	

static void (*battery_handler_chain)(float value);
static void battery_svc(float value);

void target_init(void) { 
	battery_handler_chain = battery_handler_svc;
	battery_handler_svc = battery_svc;
	ESP_LOGI(TAG, "Initializing for Muse");
}

static void battery_svc(float value) {
	ESP_LOGI(TAG, "Called for battery service with %f", value);
	// put here your code for LED according to value
	if (battery_handler_chain) battery_handler_chain(value);
}

// Battery monitoring
/*
static void battery(void *data)
{
#define VGREEN  2300
#define VRED    2000
#define NM      10
  static int val;
  static int V[NM];
  static int I=0;
  int S;
  for(int i=0;i<NM;i++)V[i]=VGREEN;
  vTaskDelay(1000 / portTICK_PERIOD_MS);	  
  struct led_state new_state;
  ws2812_control_init();
// init ADC interface for battery survey
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_GPIO33_CHANNEL, ADC_ATTEN_DB_11);
  while(true)
	{
	vTaskDelay(1000 / portTICK_PERIOD_MS);	
	V[I++] = adc1_get_raw(ADC1_GPIO33_CHANNEL);
	if(I >= NM)I = 0;
	S = 0;
	for(int i=0;i<NM;i++)S = S + V[i];	
	val = S / NM;	
	new_state.leds[0] = YELLOW;
	if(val > VGREEN) new_state.leds[0] = GREEN;	
	if(val < VRED) new_state.leds[0] = RED;
        printf("====> %d  %6x\n", val, new_state.leds[0]);	        
	ws2812_write_leds(new_state);	        

	}
}
*/

// This is the buffer which the hw peripheral will access while pulsing the output pin
rmt_item32_t led_data_buffer[LED_BUFFER_ITEMS];

void setup_rmt_data_buffer(struct led_state new_state);

void ws2812_control_init(void)
{
  rmt_config_t config;
  config.rmt_mode = RMT_MODE_TX;
  config.channel = LED_RMT_TX_CHANNEL;
  config.gpio_num = LED_RMT_TX_GPIO;
  config.mem_block_num = 3;
  config.tx_config.loop_en = false;
  config.tx_config.carrier_en = false;
  config.tx_config.idle_output_en = true;
  config.tx_config.idle_level = 0;
  config.clk_div = 2;

  ESP_ERROR_CHECK(rmt_config(&config));
  ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));
}

void ws2812_write_leds(struct led_state new_state) {
  setup_rmt_data_buffer(new_state);
  ESP_ERROR_CHECK(rmt_write_items(LED_RMT_TX_CHANNEL, led_data_buffer, LED_BUFFER_ITEMS, false));
  ESP_ERROR_CHECK(rmt_wait_tx_done(LED_RMT_TX_CHANNEL, portMAX_DELAY));
}

void setup_rmt_data_buffer(struct led_state new_state) 
{
  for (uint32_t led = 0; led < NUM_LEDS; led++) {
    uint32_t bits_to_send = new_state.leds[led];
    uint32_t mask = 1 << (BITS_PER_LED_CMD - 1);
    for (uint32_t bit = 0; bit < BITS_PER_LED_CMD; bit++) {
      uint32_t bit_is_set = bits_to_send & mask;
      led_data_buffer[led * BITS_PER_LED_CMD + bit] = bit_is_set ?
                                                      (rmt_item32_t){{{T1H, 1, TL, 0}}} : 
                                                      (rmt_item32_t){{{T0H, 1, TL, 0}}};
      mask >>= 1;
    }
  }
  }

