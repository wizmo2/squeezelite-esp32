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
typedef struct {
	int gpio;
	int active;
}  monitor_gpio_t;	

extern void (*pseudo_idle_svc)(uint32_t now);

extern void (*jack_handler_svc)(bool inserted);
extern bool jack_inserted_svc(void);

extern void (*spkfault_handler_svc)(bool inserted);
extern bool spkfault_svc(void);

extern void (*battery_handler_svc)(float value, int cells);
extern float battery_value_svc(void);
extern uint16_t battery_level_svc(void);

extern monitor_gpio_t * get_spkfault_gpio(); 
extern monitor_gpio_t * get_jack_insertion_gpio(); 

