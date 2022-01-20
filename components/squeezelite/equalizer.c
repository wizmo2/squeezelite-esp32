/* 
 *  Squeezelite for esp32
 *
 *  (c) Philippe G. 2020, philippe_44@outlook.com
 *
 *  This software is released under the MIT License.
 *  https://opensource.org/licenses/MIT
 *
 */

#include "platform_config.h"
#include "squeezelite.h" 
#include "equalizer.h"
#include "esp_equalizer.h"

#define EQ_BANDS 10
 
static log_level loglevel = lINFO;
 
static struct {
	void *handle;
	float gain[EQ_BANDS];
	bool update;
} equalizer = { .update = true };

/****************************************************************************************
 * initialize equalizer
 */
void equalizer_init(void) {
	s8_t gain[EQ_BANDS] = { };
	char *config = config_alloc_get(NVS_TYPE_STR, "equalizer");
	char *p = strtok(config, ", !");	
	
	for (int i = 0; p && i < EQ_BANDS; i++) {
		gain[i] = atoi(p);
		p = strtok(NULL, ", :");
	}
			
	free(config);	
	equalizer_update(gain);
		
	LOG_INFO("initializing equalizer");
}	
 
/****************************************************************************************
 * open equalizer
 */
void equalizer_open(u32_t sample_rate) {
	// in any case, need to clear update flag
	equalizer.update = false;
	
	if (sample_rate != 11025 && sample_rate != 22050 && sample_rate != 44100 && sample_rate != 48000) {
		LOG_WARN("equalizer only supports 11025, 22050, 44100 and 48000 sample rates, not %u", sample_rate);
		return;
	}	
	
	equalizer.handle = esp_equalizer_init(2, sample_rate, EQ_BANDS, 0);
	    
	if (equalizer.handle) {
		bool active = false;
		
		for (int i = 0; i < EQ_BANDS; i++) {
			esp_equalizer_set_band_value(equalizer.handle, equalizer.gain[i], i, 0);
			esp_equalizer_set_band_value(equalizer.handle, equalizer.gain[i], i, 1);
			active |= equalizer.gain[i] != 0;
		}
		
		// do not activate equalizer if all gain are 0
		if (!active) equalizer_close();
		
		LOG_INFO("equalizer initialized %u", active);
	} else {
		LOG_WARN("can't init equalizer");
	}	
}	

/****************************************************************************************
 * close equalizer
 */
void equalizer_close(void) {
	if (equalizer.handle) {
		esp_equalizer_uninit(equalizer.handle);
		equalizer.handle = NULL;
	}
}	

/****************************************************************************************
 * update equalizer gain
 */
void equalizer_update(s8_t *gain) {
	char config[EQ_BANDS * 4 + 1] = { };
	int n = 0;
	
	for (int i = 0; i < EQ_BANDS; i++) {
		equalizer.gain[i] = gain[i];
		n += sprintf(config + n, "%d,", gain[i]);
	}
	
	config[n-1] = '\0';
	config_set_value(NVS_TYPE_STR, "equalizer", config);					
	equalizer.update = true;
}

/****************************************************************************************
 * process equalizer 
 */
void equalizer_process(u8_t *buf, u32_t bytes, u32_t sample_rate) {
	// don't want to process with output locked, so take the small risk to miss one parametric update
	if (equalizer.update) {
		equalizer_close();
		equalizer_open(sample_rate);
	}
	
	if (equalizer.handle) {
		esp_equalizer_process(equalizer.handle, buf, bytes, sample_rate, 2);
	}	
}