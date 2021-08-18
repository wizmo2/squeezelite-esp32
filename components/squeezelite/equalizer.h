/* 
 *  Squeezelite for esp32
 *
 *  (c) Philippe G. 2020, philippe_44@outlook.com
 *
 *  This software is released under the MIT License.
 *  https://opensource.org/licenses/MIT
 *
 */
 
#pragma once

void equalizer_init(void);
void equalizer_open(u32_t sample_rate);
void equalizer_close(void);
void equalizer_update(s8_t *gain);
void equalizer_process(u8_t *buf, u32_t bytes, u32_t sample_rate);
