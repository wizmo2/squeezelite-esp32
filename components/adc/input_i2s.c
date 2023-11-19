/*
 *
 * (c) Wizmo 2023, 
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 *
 * Initalizes i2s input for loopback or streaming
 * - if host and port is provided, streaming will start on boot
 * - fmt specifies streaming format.  currently RAW or WAVE @16khz single channel.  
 * - if i2s ws etc is porvided, will create new adac on channel 1, otherwise will use existing adac (and its i2c port if applicable).
 *     adac init should configure input stream and mixer (and will define mic /line-in config).  lineinon and lineinoff added as commands.
 * - if i2s and i2c is provided, will create port, otherwise use i2c_config 
 * - playback is controlled by lms for gain, loop back and displayer
 */

#include <stdio.h>

#include "esp_pthread.h"
#include <driver/i2s.h>
//#include <esp_vad.h>

#include "input_i2s.h"
#include "gpio_exp.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_task.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "mbedtls/net_sockets.h"
#include "platform_config.h"
#include "adac.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

#define ADC_SAMPLE_RATE_HZ              16000 // default, use 16000 for Rhasspy / OpenWakeWord (OWW)
#define ADC_STREAM_FRAME_SIZE			2048 // Use 2048 for OWW (in bytes)  
#define ADC_CHANNELS_OUT				1 //  default, use 1 for OWW
#define ADC_CHANNELS_IN					2
// TODO:  Add 32bit support.
//#define BYTES_PER_FRAME					4 // 4 (2x16bit) or 8 (2x32bit)

#define ADC_I2C_PORT					1 
#define ADC_I2S_NUM                      I2S_NUM_1

#define ADC_STACK_SIZE 	(4*1024)

#define SAFE_PTR_FREE(P)							\
	do {											\
		TimerHandle_t timer = xTimerCreate("cleanup", pdMS_TO_TICKS(10000), pdFALSE, P, _delayed_free);	\
		xTimerStart(timer, portMAX_DELAY);			\
	} while (0)				
static void inline _delayed_free(TimerHandle_t xTimer) {
	free(pvTimerGetTimerID(xTimer));
	xTimerDelete(xTimer, portMAX_DELAY);
}


const struct adac_s *aadc;

typedef struct adc_ctx_s {
	uint32_t host;			// ip address of udp server/receiver
	uint16_t port;			// stream port
	uint16_t channels;		// stream output channels
	adc_fmt_t format;		// stream format (currently raw and wav chunks.  TODO:  mp3/flac streams)
	int sock;               // stream socket
	
	bool running; 			// thread running
	bool pause;				// stop i2s read
	bool speaker; 			// send to dac
	bool stream; 			// sedn udp stream
	
	TaskHandle_t thread, joiner;
	StaticTask_t *xTaskBuffer;
	StackType_t xStack[ADC_STACK_SIZE] __attribute__ ((aligned (4)));

	uint16_t i2s_num;		
	uint16_t sample_rate;	// input, output, and stream rate (Hz)
	int16_t *input_buff;	
	int16_t *stream_buff;
	size_t bytes_read;		
	adc_cmd_cb_t	cmd_cb;
	adc_data_cb_t	data_cb;
	void *owner;

} adc_ctx_t;

static const char TAG[] = "input_i2c";
char title[20];

static void	adc_thread(void *arg);

/*----------------------------------------------------------------------------*/
struct adc_ctx_s *adc_create(adc_cmd_cb_t cmd_cb, adc_data_cb_t data_cb) {

	struct adc_ctx_s *ctx = malloc(sizeof(struct adc_ctx_s));
	if (!ctx) return NULL;

	// make sure we have a clean context
	memset(ctx, 0, sizeof(adc_ctx_t));

	ctx->cmd_cb = cmd_cb;
	ctx->data_cb = data_cb;

	// Load configration from NVS
	char* config = config_alloc_get_str("adc_stream", NULL, " ");
    ctx->sample_rate = ADC_SAMPLE_RATE_HZ;
	PARSE_PARAM(config, "rate",'=', ctx->sample_rate);
    char host[32];
    PARSE_PARAM_STR(config, "host", '=', host, 32);
	ctx->host = inet_addr(host);
	PARSE_PARAM(config, "port",'=', ctx->port);
	ctx->channels = ADC_CHANNELS_OUT;
	PARSE_PARAM(config, "ch",'=', ctx->channels);
	PARSE_PARAM(config, "fmt", '=', ctx->format);
	free(config);

	config = config_alloc_get_str("adc_config", NULL, CONFIG_ADC_CONFIG);
	char model[32] = "\0";
	PARSE_PARAM_STR(config, "model", '=', model, 31);
	if (model[0] != 0) {
		ctx->i2s_num = ADC_I2S_NUM;
		aadc  = &dac_external;
	} 
		
	if (aadc) {
		i2s_pin_config_t pin_config = {.bck_io_num=-1, .ws_io_num=-1, .data_out_num=-1, .data_in_num=-1, .mck_io_num=-1 };
		PARSE_PARAM(config, "bck",'=', pin_config.bck_io_num);
		PARSE_PARAM(config, "ws",'=', pin_config.ws_io_num);
		PARSE_PARAM(config, "do",'=', pin_config.data_out_num);
		PARSE_PARAM(config, "di",'=', pin_config.data_in_num);
		PARSE_PARAM(config, "mck",'=', pin_config.mck_io_num);

		ESP_LOGI( TAG, "%s ADC using I2S ch %d bck:%u, ws:%u, di:%u (mlk:%u)", model, ADC_CHANNELS_IN, pin_config.bck_io_num, pin_config.ws_io_num, pin_config.data_in_num, pin_config.mck_io_num);

		i2s_config_t i2s_config = {
			.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
			.sample_rate = ctx->sample_rate,
			.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
			.channel_format = (ADC_CHANNELS_IN==2 ? I2S_CHANNEL_FMT_RIGHT_LEFT : I2S_CHANNEL_FMT_ONLY_LEFT),
			.communication_format = I2S_COMM_FORMAT_STAND_I2S,
			.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
			.dma_buf_count = 4, //8,
			.dma_buf_len = 128, //64,
			.use_apll = true,
			.tx_desc_auto_clear = true,
			.fixed_mclk = 0,
			.mclk_multiple = I2S_MCLK_MULTIPLE_256,
			.bits_per_chan = I2S_BITS_PER_CHAN_16BIT,
#ifndef CONFIG_IDF_TARGET_ESP32
			.chan_mask = (i2s_channel_t)(I2S_TDM_ACTIVE_CH0 | I2S_TDM_ACTIVE_CH1),
#endif
		};

		uint32_t res = ESP_OK;
		bool mck_required = false;
		res = aadc->init(config, ADC_I2C_PORT, &i2s_config, &mck_required) ? ESP_OK : ESP_FAIL;
		if (res != ESP_OK) {
			ESP_LOGE(TAG, "Error initializing i2c on ADC");
		}
#ifdef CONFIG_IDF_TARGET_ESP32
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 4, 0)        
		int mck_io_num = strcasestr(dac_config, "mck") || mck_required ? 0 : -1;
		PARSE_PARAM(dac_config, "mck", '=', mck_io_num);

		ESP_LOGI(TAG, "configuring MCLK on GPIO %d", mck_io_num);

		if (mck_io_num == GPIO_NUM_0) {
			PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
			WRITE_PERI_REG(PIN_CTRL, CONFIG_DAC_I2S_NUM == I2S_NUM_0 ? 0xFFF0 : 0xFFFF);
		} else if (mck_io_num == GPIO_NUM_1) {
			PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD_CLK_OUT3);
			WRITE_PERI_REG(PIN_CTRL, CONFIG_DAC_I2S_NUM == I2S_NUM_0 ? 0xF0F0 : 0xF0FF);
		} else if (mck_io_num == GPIO_NUM_2) {
			PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_U0RXD_CLK_OUT2);
			WRITE_PERI_REG(PIN_CTRL, CONFIG_DAC_I2S_NUM == I2S_NUM_0 ? 0xFF00 : 0xFF0F);
		} else {
			LOG_WARN("invalid MCK gpio %d", mck_io_num);
		}
#else
		if (mck_required && pin_config.mck_io_num == -1) pin_config.mck_io_num = 0;
		ESP_LOGI(TAG, "configuring MCLK on GPIO %d", pin_config.mck_io_num);
#endif
#endif
		ESP_LOGI( TAG, "Initializing ADC I2S with rate: %u, bits per sample: %u, buffer frames: %u, number of buffers: %u",
		      i2s_config.sample_rate, i2s_config.bits_per_sample, i2s_config.dma_buf_count, i2s_config.dma_buf_len) ;

		/* Start I2s for read */
		i2s_driver_install(ADC_I2S_NUM, &i2s_config, 0, NULL);
		i2s_set_pin(ADC_I2S_NUM, &pin_config);
		i2s_zero_dma_buffer(ADC_I2S_NUM);

	} else {
		// No dedicated ADC chip
		ESP_LOGI( TAG, "ADC sharing not currently available");
	}

	if (ctx->host && ctx->port) {
		/* Initilaize output stream */
		int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
		if (sock < 0) {
			ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
			free(ctx);
			return NULL;
		}
		
		int enable = 1;
		int err = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
		if (err != 0) {
			ESP_LOGW(TAG, "Socket unable to set reuseaddr: errno %d", err);
		}

		//set to non-blocking mode
		int flags = fcntl(sock, F_GETFL, 0);	
		err = fcntl(sock, F_SETFL, flags | O_NONBLOCK);
		//err = ioctl(sock, FIONBIO, &enable);
		if (err != 0) {
			ESP_LOGW(TAG, "Socket unable to set nonblocking mode: errno %d", err);
			free(ctx);
			return NULL;
		}
		
		ctx->sock = sock;
	
		ESP_LOGI(TAG, "Configured ADC stream to %s:%d fmt:%s", host, ctx->port, ctx->format?"WAVE":"RAW");
	}

	ctx->running = true;
	ctx->cmd_cb(ADC_SETUP, ctx->sample_rate); 

    ctx->xTaskBuffer = (StaticTask_t*) heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
	ctx->thread = xTaskCreateStaticPinnedToCore( (TaskFunction_t) adc_thread, "ADC", ADC_STACK_SIZE, ctx, 
												 ESP_TASK_PRIO_MIN + 2, ctx->xStack, ctx->xTaskBuffer, CONFIG_PTHREAD_TASK_CORE_DEFAULT);

	return ctx;
}

/*----------------------------------------------------------------------------*/
void adc_abort(struct adc_ctx_s *ctx) {
	ESP_LOGI(TAG, "[%p]: aborting ADC session at next select() wakeup", ctx);
	ctx->running = false;
}	

/*----------------------------------------------------------------------------*/
void adc_delete(struct adc_ctx_s *ctx) {
	// ADD I2S CLEAN UP NAN_CONCAT_HELPER
	ESP_LOGI(TAG, "[%p]: deleting ADC session", ctx);

	if (!ctx) return;

	// then the task
	ctx->joiner = xTaskGetCurrentTaskHandle();
	ctx->running = false;
	
	// brute-force exit of accept() 
	shutdown(ctx->sock, SHUT_RDWR);
	closesocket(ctx->sock);

	// wait to make sure LWIP if scheduled (avoid issue with NotifyTake)	
	vTaskDelay(100 / portTICK_PERIOD_MS);
	ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
	vTaskDelete(ctx->thread);
	SAFE_PTR_FREE(ctx->xTaskBuffer);

	free(ctx);
}

/*----------------------------------------------------------------------------*/
bool adc_cmd(struct adc_ctx_s *ctx, adc_event_t event, void *param) {

	ESP_LOGI(TAG, "ADC_cmd %d", event );

	bool success = true;

	// first notify the remote controller (if any)
	switch(event) {
		case ADC_SETUP:
			ESP_LOGI(TAG, "ADC Setup");
			ctx->stream = true;
			ctx->cmd_cb(ADC_SETUP, ctx->sample_rate);
			break;
		case ADC_PLAY:
		    ESP_LOGI(TAG, "ADC Play");
			ctx->speaker = true;
			ESP_LOGI(TAG, "About to start play cb");
			ctx->cmd_cb(ADC_PLAY);
			break;
		case ADC_STREAM:
		    ESP_LOGI(TAG, "ADC Stream");
			ctx->stream = true;
			break;
		case ADC_STOP:
		    ESP_LOGI(TAG, "ADC Stop");
			ctx->speaker = false;
			break;
		case ADC_CLOSE:
			ESP_LOGI(TAG, "ADC Close");
			ctx->speaker = false;
			break;
		default:
			ESP_LOGI(TAG, "ADC Unknown: %d", event);
			break;
	}

	return success;
}

#define MAX_HEADER_SIZE		128
#define WAVE_HEADER_SIZE	44
/*----------------------------------------------------------------------------
   Fills in the header based on sample size                                 
*/
static uint16_t generate_wav_header(char* wav_header, uint32_t wav_size, uint32_t sample_rate, uint16_t channels){

    // See this for reference: http://soundfile.sapp.org/doc/WaveFormat/
    uint32_t file_size = wav_size + WAVE_HEADER_SIZE - 8;
    uint32_t byte_rate = sample_rate * 16 * channels / 8;

    const char set_wav_header[] = {
        'R','I','F','F', // ChunkID
        file_size, file_size >> 8, file_size >> 16, file_size >> 24, // ChunkSize
        'W','A','V','E', // Format
        'f','m','t',' ', // Subchunk1ID
        0x10, 0x00, 0x00, 0x00, // Subchunk1Size (16 for PCM)
        0x01, 0x00, // AudioFormat (1 for PCM)
        channels, channels >> 8, // NumChannels
        sample_rate, sample_rate >> 8, sample_rate >> 16, sample_rate >> 24, // SampleRate
        byte_rate, byte_rate >> 8, byte_rate >> 16, byte_rate >> 24, // ByteRate
        0x02, 0x00, // BlockAlign
        0x10, 0x00, // BitsPerSample (16 bits)
        'd','a','t','a', // Subchunk2ID
        wav_size, wav_size >> 8, wav_size >> 16, wav_size >> 24, // Subchunk2Size
    };

    memcpy(wav_header, set_wav_header, sizeof(set_wav_header));

	return WAVE_HEADER_SIZE;
}
/*----------------------------------------------------------------------------
   Converts stereo data to mono and adds to wave file   
   TODO: higher bit rate output needs encoding (shine/libflac?)                  
*/
static uint16_t encode_wav_data(int16_t* dst, int16_t* src, uint32_t offset, size_t size, uint16_t channels) {
	int s = sizeof(short);
    int p = offset/s; // start of frame data

	if (ADC_CHANNELS_IN == channels) {
		memcpy(&dst[p], src, size*s);
		p+=size;
	} else {
		for (int i=0; i < size; i+=2) {
			dst[p++] = (src[i] / 2) + (src[i+1] / 2); 
		}
	}
	return p*s;
}

/*----------------------------------------------------------------------------*/
static void adc_thread(void *arg) {
	// THIS IS THE ORIGIONAL CODE
	adc_ctx_t *ctx = (adc_ctx_t*) arg;

	size_t buffer_size_bytes = ADC_STREAM_FRAME_SIZE * ADC_CHANNELS_IN / ctx->channels;
	size_t buffer_size = buffer_size_bytes / sizeof(short); 
	ctx->input_buff = (int16_t *)malloc(buffer_size_bytes);
    if (ctx->input_buff == NULL) {
		ESP_LOGE(TAG, "Memory Allocation Failed!");
		free(ctx);
		return;
    }
	char *buf_ptr_read = (char *)ctx->input_buff;

	size_t stream_size_bytes = ADC_STREAM_FRAME_SIZE;
	ctx->stream_buff = (int16_t *)malloc(MAX_HEADER_SIZE + stream_size_bytes);
    if (ctx->stream_buff == NULL) {
		ESP_LOGE(TAG, "Stream buffer Memory Allocation Failed!");
		ctx->stream = false;
    }
	char *buf_ptr_stream = (char *)ctx->stream_buff;
	
	int stream_byte_ptr = 0; 
	size_t header_size = 0;
	if (ctx->format == ADC_FMT_WAV) { 
		stream_byte_ptr = generate_wav_header(buf_ptr_stream, stream_size_bytes, ctx->sample_rate, ctx->channels);
		header_size = (size_t)stream_byte_ptr;
	}

	struct sockaddr_in dest_addr;
	dest_addr.sin_addr.s_addr = ctx->host;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(ctx->port); 
	
	ESP_LOGI(TAG, "Initialized ADC stream: rate:%u (%ums), channels:%u bytes:%u" , ctx->sample_rate, buffer_size * 1000 / (ctx->sample_rate * ADC_CHANNELS_IN), ctx->channels, stream_size_bytes);

	int stream_errs = 0;

	while (ctx->running) {
        
		// TODO: This does not work with shared chip!  (but recovers) 
		//   issues associated modifying sample_rate that is used by both adc and dac.
        if (i2s_read(ctx->i2s_num, buf_ptr_read, buffer_size_bytes, &ctx->bytes_read, portMAX_DELAY) == ESP_OK) {
			if (ctx->speaker && ctx->bytes_read > 0)
			{
				ctx->data_cb((const uint8_t*) ctx->input_buff, ctx->bytes_read);
				//ESP_LOGD(TAG, "Sent Bytes %d to DAC",ctx->bytes_read);
			}

			if (ctx->sock && ctx->stream && ctx->bytes_read == buffer_size_bytes)
			{
            	stream_byte_ptr = encode_wav_data(ctx->stream_buff, ctx->input_buff, (size_t)stream_byte_ptr, buffer_size, ctx->channels);
				
				//ESP_LOGD(TAG, "%d,%db %u Read, Sending bytes to Stream %s:%d",ctx->bytes_read, stream_byte_ptr, buffer_size, inet_ntoa(dest_addr.sin_addr), htons (dest_addr.sin_port));
				int err = sendto(ctx->sock, buf_ptr_stream, (size_t)stream_byte_ptr, 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr));
				if (stream_byte_ptr >= stream_size_bytes) {
            		if (err < 0 && stream_errs++ > 10) {
                		ESP_LOGE(TAG, "Multiple errors occurred during sending: check 'CONFIG_LWIP_IP4_FRAG=y'");
						stream_errs = 0;
						//const TickType_t xDelay = 10 / portTICK_PERIOD_MS;
						//vTaskDelay(xDelay);
            		} else {
						stream_errs = 0;
						stream_byte_ptr = header_size;
					}
				}
			}
        } else {
            ESP_LOGI(TAG, "Read Failed!");
        }
	}
	
	if (ctx->sock != -1) closesocket(ctx->sock);

	xTaskNotifyGive(ctx->joiner);
	vTaskSuspend(NULL);
}

