/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#ifndef ADC_SINK_H
#define ADC_SINK_H

#include <stdint.h>
#include <stdarg.h>


typedef enum { 	ADC_SETUP, ADC_STREAM, ADC_PLAY, ADC_FLUSH, ADC_STOP, ADC_STALLED, 
				ADC_TOGGLE, ADC_CLOSE, ADC_METADATA, ADC_GAIN } adc_event_t ;  // , ADC_VOLUME_UP, ADC_VOLUME_DOWN, 

typedef bool (*adc_cmd_cb_t)(adc_event_t event, ...);
typedef bool (*adc_cmd_vcb_t)(adc_event_t event, va_list args);
typedef void (*adc_data_cb_t)(const uint8_t *data, size_t len);

/**
 * @brief     init sink mode (need to be provided)
 */
void adc_sink_init(adc_cmd_vcb_t cmd_cb, adc_data_cb_t data_cb);

/**
 * @brief     deinit sink mode (need to be provided)
 */
void adc_sink_deinit(void);

/**
 * @brief     force disconnection
 */
void adc_disconnect(void);
void adc_linein_start(uint16_t sample_rate);

#endif /* ADC_SINK_H*/