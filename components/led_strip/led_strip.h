/*  ---------------------------------------------------------------------------
    File: led_strip.h
    Author(s):  Lucas Bruder <LBruder@me.com>
    Date Created: 11/23/2016
    Last modified: 09/24/2023

    Updated:  Wizmo - ad support for APA102 

    Description: 
    This library can drive led strips through the RMT module on the ESP32.
    ------------------------------------------------------------------------ */

#ifndef LED_STRIP_H
#define LED_STRIP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <driver/rmt.h>
#include <driver/gpio.h>
#include "freertos/semphr.h"
#include <stddef.h>

enum rgb_led_type_t {
    RGB_LED_TYPE_WS2812 = 0,
    RGB_LED_TYPE_SK6812 = 1,
    RGB_LED_TYPE_APA106 = 2,
    RGB_LED_TYPE_APA102 = 3,
    RGB_LED_TYPE_MAX,
};

/**
 * RGB LED colors
 */
struct led_color_t {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

#define LED_STRIP_SEQ_MAX_SIZE 10

struct led_strip_t {
    enum rgb_led_type_t rgb_led_type; // should be const, but workaround needed for initialization
    uint32_t led_strip_length;

    // RMT peripheral settings
    rmt_channel_t rmt_channel;
    
    gpio_num_t gpio; // Must be less than GPIO_NUM_33
    gpio_num_t clk; // APA102 only 
    uint8_t seq[LED_STRIP_SEQ_MAX_SIZE]; // APA102 only

    struct led_color_t *led_strip_working;
    struct led_color_t *led_strip_showing;

    SemaphoreHandle_t access_semaphore;
};

bool led_strip_init(struct led_strip_t *led_strip);

/**
 * Sets the pixel at pixel_num to color.
 */
bool led_strip_set_pixel_color(struct led_strip_t *led_strip, uint32_t pixel_num, struct led_color_t *color);
bool led_strip_set_pixel_rgb(struct led_strip_t *led_strip, uint32_t pixel_num, uint8_t red, uint8_t green, uint8_t blue);
/**
 * Get the pixel color at pixel_num for the led strip that is currently being shown! 
 * NOTE: If you call set_pixel_color then get_pixel_color for the same pixel_num, you will not 
 * get back the same pixel value. This gets you the color of the pixel currently being shown, not the one
 * being updated
 *
 * If there is an invalid argument, color will point to NULL and this function will return false.
 */
bool led_strip_get_pixel_color(struct led_strip_t *led_strip, uint32_t pixel_num, struct led_color_t *color);

/**
 * Updates the led buffer to be shown using double buffering.
 */
bool led_strip_show(struct led_strip_t *led_strip);

/**
 * Clears the LED strip.
 */
bool led_strip_clear(struct led_strip_t *led_strip);

#ifdef __cplusplus
}
#endif

#endif // LED_STRIP_H
