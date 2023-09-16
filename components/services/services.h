/* 
 *  Squeezelite for esp32
 *
 *  (c) Philippe G. 2019, philippe_44@outlook.com
 *
 *  This software is released under the MIT License.
 *  https://opensource.org/licenses/MIT
 *
 */
 
#pragma once

typedef enum { SLEEP_ONTIMER, SLEEP_ONKEY, SLEEP_ONGPIO, SLEEP_ONIR } sleep_cause_e;
void services_sleep_activate(sleep_cause_e cause);
void services_sleep_sethook(void (*hook)(void));
void services_sleep_callback(uint32_t elapsed);
