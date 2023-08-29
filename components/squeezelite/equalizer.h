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
void equalizer_close(void);
void equalizer_set_samplerate(uint32_t samplerate);
void equalizer_set_gain(int8_t *gain);
void equalizer_set_loudness(u8_t loudness);
void equalizer_set_volume(unsigned left, unsigned right);
void equalizer_process(uint8_t *buf, uint32_t bytes);
