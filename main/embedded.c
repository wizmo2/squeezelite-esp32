/* 
 *  Squeezelite for esp32
 *
 *  (c) Sebastien 2019
 *      Philippe G. 2019, philippe_44@outlook.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include "squeezelite.h"
#include "esp_pthread.h"
#include "esp_system.h"

void get_mac(u8_t mac[]) {
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
}

_sig_func_ptr signal(int sig, _sig_func_ptr func) {
	return NULL;
}

void *audio_calloc(size_t nmemb, size_t size) {
		return calloc(nmemb, size);
}

int pthread_setname_np(pthread_t thread, const char *name) { 
	esp_pthread_cfg_t cfg = esp_pthread_get_default_config(); 
	cfg.thread_name= name; 
	cfg.inherit_cfg = true; 
	return esp_pthread_set_cfg(&cfg); 
}


