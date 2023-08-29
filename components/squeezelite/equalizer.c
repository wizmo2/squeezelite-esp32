/*
 *  Squeezelite for esp32
 *
 *  (c) Philippe G. 2020, philippe_44@outlook.com
 *
 *  This software is released under the MIT License.
 *  https://opensource.org/licenses/MIT
 *
 */

#include "math.h"
#include "platform_config.h"
#include "squeezelite.h"
#include "equalizer.h"
#include "esp_equalizer.h"

#define EQ_BANDS 10

static log_level loglevel = lINFO;

static EXT_RAM_ATTR struct {
	void *handle;
    float loudness, volume;
    uint32_t samplerate;
	float gain[EQ_BANDS], loudness_gain[EQ_BANDS];
	bool update;
} equalizer;

#define POLYNOME_COUNT 6

static const float loudness_envelope_coefficients[EQ_BANDS][POLYNOME_COUNT] = {
 {5.5169301499257067e+001, 6.3671410796029004e-001,
  -4.2663226432095233e-002, 8.1063072336581246e-004,
  -7.3621858933917722e-006, 2.5349489594339575e-008},
 {3.7716143859944118e+001, 1.2355293276538579e+000,
  -6.6435374582217863e-002, 1.2976763440259382e-003,
  -1.1978732496353172e-005, 4.1664114634622593e-008},
 {2.5103632377146837e+001, 1.3259150615414637e+000,
  -6.6332442135695099e-002, 1.2845279812261677e-003,
  -1.1799885217545631e-005, 4.0925911584040685e-008},
 {1.3159168212144563e+001, 8.8149357628440639e-001,
  -4.0384121097225931e-002, 7.3843501027501322e-004,
  -6.5508794453097008e-006, 2.2221997141120518e-008},
 {5.1337853800151700e+000, 4.0817077967582394e-001,
  -1.4107826528626457e-002, 1.5251066311713760e-004,
  -3.6689819583740298e-007, -2.0390798774727989e-009},
 {3.1432364156464315e-001, 9.1260548140023004e-002,
  -3.5012124633183438e-004, -8.6023911664606992e-005,
  1.6785606828245921e-006, -8.8269731094371646e-009},
 {-4.0965062397075833e+000, 1.3667010948271402e-001,
  2.4775896786988390e-004, -9.6620399661858641e-005,
  1.7733690952379155e-006, -9.1583104942496635e-009},
 {-9.0275786029994176e+000, 2.6226938845184250e-001,
  -6.5777547972402156e-003, 1.0045957188977551e-004,
  -7.8851000325128971e-007, 2.4639885209682384e-009},
 {-4.4275018199195815e+000, 4.5399572638241725e-001,
  -2.4034902766833462e-002, 5.9828953622534668e-004,
  -6.2893971217140864e-006, 2.3133296592719627e-008},
 {1.4243299202697818e+001, 3.6984458807056630e-001,
  -3.0413994109395680e-002, 7.6700105080386904e-004,
  -8.2777185209388079e-006, 3.1352890650784970e-008} };

/****************************************************************************************
 * calculate loudness gains
 */
static void calculate_loudness(void) {      
	for (int i = 0; i < EQ_BANDS; i++) {
		for (int j = 0; j < POLYNOME_COUNT && equalizer.loudness != 0; j++) {
			equalizer.loudness_gain[i] +=
				loudness_envelope_coefficients[i][j] * pow(equalizer.volume, j);
		}
		equalizer.loudness_gain[i] *= equalizer.loudness;
	}
}

/****************************************************************************************
 * initialize equalizer
 */
void equalizer_init(void) {
    // handle equalizer
	char *config = config_alloc_get(NVS_TYPE_STR, "equalizer");
	char *p = strtok(config, ", !");

	for (int i = 0; p && i < EQ_BANDS; i++) {
		equalizer.gain[i] = atoi(p);
		p = strtok(NULL, ", :");
	}

	free(config);

    // handle loudness
    config = config_alloc_get(NVS_TYPE_STR, "loudness");
    equalizer.loudness = atof(config) / 100.0;

	free(config);
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
 * change sample rate
 */
void equalizer_set_samplerate(uint32_t samplerate) {
    if (equalizer.samplerate != samplerate) equalizer_close();
    equalizer.samplerate = samplerate;
    equalizer.update = true;

    LOG_INFO("equalizer sample rate %u", samplerate);
}

/****************************************************************************************
 * get volume update and recalculate loudness according to
 */
void equalizer_set_volume(unsigned left, unsigned right) {
	equalizer.volume = (left + right) / 2;
    // do classic dB conversion and scale it 0..100
	if (equalizer.volume) equalizer.volume = log2(equalizer.volume);
	equalizer.volume = equalizer.volume / 16.0 * 100.0;
	calculate_loudness();
    equalizer.update = true;
}

/****************************************************************************************
 * change gains from LMS
 */
void equalizer_set_gain(int8_t *gain) {
    char config[EQ_BANDS * 4 + 1] = { };
	int n = 0;

	for (int i = 0; i < EQ_BANDS; i++) {
		equalizer.gain[i] = gain[i];
		n += sprintf(config + n, "%d,", gain[i]);
	}

	config[n-1] = '\0';
	config_set_value(NVS_TYPE_STR, "equalizer", config);
	equalizer.update = true;

    LOG_INFO("equalizer gain %s", config);
}

/****************************************************************************************
 * change loudness from LMS
 */
void equalizer_set_loudness(uint8_t loudness) {
    // update loudness gains as a factor of loudness and volume
	equalizer.loudness = loudness / 100.0;
    calculate_loudness();

    char p[4];
    itoa(loudness, p, 10);
    config_set_value(NVS_TYPE_STR, "loudness", p);
    equalizer.update = true;

    LOG_INFO("equalizer loudness %u", (unsigned) loudness);
}

/****************************************************************************************
 * process equalizer
 */
void equalizer_process(uint8_t *buf, uint32_t bytes) {
	// don't want to process with output locked, so take the small risk to miss one parametric update
	if (equalizer.update) {
        equalizer.update = false;

        if (equalizer.samplerate != 11025 && equalizer.samplerate != 22050 && equalizer.samplerate != 44100 && equalizer.samplerate != 48000) {
            LOG_WARN("equalizer only supports 11025, 22050, 44100 and 48000 sample rates, not %u", equalizer.samplerate);
            return;
        }

        if (!equalizer.handle && ((equalizer.handle = esp_equalizer_init(2, equalizer.samplerate, EQ_BANDS, 0)) == NULL)) {
            LOG_WARN("can't init equalizer");
            return;
        }

		bool active = false;
		for (int i = 0; i < EQ_BANDS; i++) {
            float gain = equalizer.gain[i] + equalizer.loudness_gain[i];
			esp_equalizer_set_band_value(equalizer.handle, gain, i, 0);
			esp_equalizer_set_band_value(equalizer.handle, gain, i, 1);
			active |= gain != 0;
            LOG_INFO("EQUALIZER INDEX %u => gain:%.2f, loudness:%.2f,", i, equalizer.gain[i], equalizer.loudness_gain[i]);
		}

		// at the end do not activate equalizer if all gain are 0
		if (!active) equalizer_close();
		LOG_INFO("equalizer %s", active ? "actived" : "deactivated");
	}

	if (equalizer.handle) {
		esp_equalizer_process(equalizer.handle, buf, bytes, equalizer.samplerate, 2);
	}
}