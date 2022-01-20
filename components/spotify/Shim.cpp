/* 
 *  This software is released under the MIT License.
 *  https://opensource.org/licenses/MIT
 *
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_server.h"

#include <ConstantParameters.h>
#include <Session.h>
#include <SpircController.h>
#include <MercuryManager.h>
#include <ZeroconfAuthenticator.h>
#include <ApResolve.h>
#include <HTTPServer.h>
#include "ConfigJSON.h"
#include "Logger.h"

#include "platform_config.h"
#include "tools.h"
#include "cspot_private.h"
#include "cspot_sink.h"
#include "Shim.h"

extern "C" {
	httpd_handle_t get_http_server(int *port);
	static esp_err_t handlerWrapper(httpd_req_t *req);
};

#define CSPOT_STACK_SIZE (8*1024)

static const char *TAG = "cspot";

// using a global is pretty ugly, but it's easier with all Lambda below
static EXT_RAM_ATTR struct cspot_s {
	char name[32];
	cspot_cmd_cb_t cHandler;
	cspot_data_cb_t dHandler;
	TaskHandle_t TaskHandle;
    std::shared_ptr<LoginBlob> blob;
} cspot;

std::shared_ptr<ConfigJSON> configMan;
std::shared_ptr<NVSFile> file;
std::shared_ptr<MercuryManager> mercuryManager;
std::shared_ptr<SpircController> spircController;

/****************************************************************************************
 * Main task (could it be deleted after spirc has started?)
 */
static void cspotTask(void *pvParameters) {
	char configName[] = "cspot_config";
	std::string jsonConfig;	
	
    // Config file
    file = std::make_shared<NVSFile>();
	configMan = std::make_shared<ConfigJSON>(configName, file);
   
	// We might have no config at all
	if (!file->readFile(configName, jsonConfig) || !jsonConfig.length()) {
		ESP_LOGW(TAG, "Cannot load config, using default");
		
		configMan->deviceName = cspot.name;
		configMan->format = AudioFormat_OGG_VORBIS_160;
		configMan->volume = 32767;

		configMan->save();	
	}
	
	// safely load config now
	configMan->load();
	if (!configMan->deviceName.length()) configMan->deviceName = cspot.name;
	ESP_LOGI(TAG, "Started CSpot with %s (bitrate %d)", configMan->deviceName.c_str(), configMan->format == AudioFormat_OGG_VORBIS_320 ? 320 : (configMan->format == AudioFormat_OGG_VORBIS_160 ? 160 : 96));

	// All we do here is notify the task to start the mercury loop
    auto createPlayerCallback = [](std::shared_ptr<LoginBlob> blob) {
		// TODO: handle/refuse that another user takes ownership
		cspot.blob = blob;
		xTaskNotifyGive(cspot.TaskHandle);
    };

	int port;
	httpd_handle_t server = get_http_server(&port);
	auto httpServer = std::make_shared<ShimHTTPServer>(server, port);

    auto authenticator = std::make_shared<ZeroconfAuthenticator>(createPlayerCallback, httpServer);
	authenticator->registerHandlers();

	// wait to be notified and have a mercury loop
	while (1) {
		ulTaskNotifyTake(pdFALSE, portMAX_DELAY);

        auto session = std::make_unique<Session>();
        session->connectWithRandomAp();
        auto token = session->authenticate(cspot.blob);

        ESP_LOGI(TAG, "Creating Spotify (using CSpot) player");
		
        // Auth successful
        if (token.size() > 0 && cspot.cHandler(CSPOT_SETUP, 44100)) {
			auto audioSink = std::make_shared<ShimAudioSink>();

            mercuryManager = std::make_shared<MercuryManager>(std::move(session));
            mercuryManager->startTask();

            spircController = std::make_shared<SpircController>(mercuryManager, cspot.blob->username, audioSink);

			spircController->setEventHandler([](CSpotEvent &event) {
            switch (event.eventType) {
            case CSpotEventType::TRACK_INFO: {
                TrackInfo track = std::get<TrackInfo>(event.data);
				cspot.cHandler(CSPOT_TRACK, 44100, track.duration, track.artist.c_str(), 
							   track.album.c_str(), track.name.c_str(), track.imageUrl.c_str());
                break;
            }
            case CSpotEventType::PLAY_PAUSE: {
                bool isPaused = std::get<bool>(event.data);
				if (isPaused) cspot.cHandler(CSPOT_PAUSE);
				else cspot.cHandler(CSPOT_PLAY);
                break;
            }
			case CSpotEventType::LOAD:
				cspot.cHandler(CSPOT_LOAD, std::get<int>(event.data), -1);
				break;
			case CSpotEventType::SEEK:
				cspot.cHandler(CSPOT_SEEK, std::get<int>(event.data));
				break;
			case CSpotEventType::DISC:
				cspot.cHandler(CSPOT_DISC);				
				spircController->stopPlayer();
				mercuryManager->stop();
				break;
			case CSpotEventType::PREV:
			case CSpotEventType::NEXT:
				cspot.cHandler(CSPOT_FLUSH);
                break;
			/*
			// we use volume from sink which is a 16 bits value
			case CSpotEventType::VOLUME: {
                int volume = std::get<int>(event.data);
				cspot.cHandler(CSPOT_VOLUME, volume);
				ESP_LOGW(TAG, "cspot volume : %d", volume);
                break;
            }
			*/
            default:
                break;
            }
			});

            mercuryManager->reconnectedCallback = []() {
                return spircController->subscribe();
            };

            mercuryManager->handleQueue();
        
			// release controllers
			mercuryManager.reset();
			spircController.reset();
		}

		// release auth blob and flush files
		cspot.blob.reset();
		file->flush();

		ESP_LOGI(TAG, "Shutting down CSpot player");
	}

	// we should not be here
	vTaskDelete(NULL);
}

/****************************************************************************************
 * API to create and start a cspot instance
 */
struct cspot_s* cspot_create(const char *name, cspot_cmd_cb_t cmd_cb, cspot_data_cb_t data_cb) {
	static DRAM_ATTR StaticTask_t xTaskBuffer __attribute__ ((aligned (4)));
	static EXT_RAM_ATTR StackType_t xStack[CSPOT_STACK_SIZE] __attribute__ ((aligned (4)));

	bell::setDefaultLogger();
	
	cspot.cHandler = cmd_cb;
	cspot.dHandler = data_cb;
	strncpy(cspot.name, name, sizeof(cspot.name) - 1);
    cspot.TaskHandle = xTaskCreateStatic(&cspotTask, "cspot", CSPOT_STACK_SIZE, NULL, CONFIG_ESP32_PTHREAD_TASK_PRIO_DEFAULT - 2, xStack, &xTaskBuffer);
	
	return &cspot;
}

/****************************************************************************************
 * Commands sent by local buttons/actions
 */
bool cspot_cmd(struct cspot_s* ctx, cspot_event_t event, void *param) {
	// we might have not controller left
	if (!spircController.use_count()) return false;

	switch(event) {
		case CSPOT_PREV:
			spircController->prevSong();
			break;
		case CSPOT_NEXT:
			spircController->nextSong();
			break;
		case CSPOT_TOGGLE:
			spircController->playToggle();
			break;
		case CSPOT_PAUSE:
			spircController->setPause(true);
			break;
		case CSPOT_PLAY:
			spircController->setPause(false);
			break;
		case CSPOT_DISC:
			spircController->disconnect();
			break;
		case CSPOT_STOP:
			spircController->stopPlayer();
			break;
		case CSPOT_VOLUME_UP:
			spircController->adjustVolume(MAX_VOLUME / 100 + 1);
			break;
		case CSPOT_VOLUME_DOWN:
			spircController->adjustVolume(-(MAX_VOLUME / 100 + 1));
			break;
		default:
			break;
	}

	return true;
}

/****************************************************************************************
 * AudioSink class to push data to squeezelite backend (decode_external)
 */
void ShimAudioSink::volumeChanged(uint16_t volume) {
	cspot.cHandler(CSPOT_VOLUME, volume);
}

void ShimAudioSink::feedPCMFrames(const uint8_t *data, size_t bytes) {	
	cspot.dHandler(data, bytes);
}

/****************************************************************************************
 * NVSFile class to store config
 */
 bool NVSFile::readFile(std::string filename, std::string &fileContent) {
	auto search = files.find(filename);
    
	// cache 
	if (search == files.end()) {
		char *content = (char*) config_alloc_get(NVS_TYPE_STR, filename.c_str());
		if (!content) return false;
		fileContent = content;
		free(content);
	} else {
		fileContent = search->second;
	}

	return true;
}

bool NVSFile::writeFile(std::string filename, std::string fileContent) {
    auto search = files.find(filename);

	files[filename] = fileContent;    
	if (search == files.end()) return (ESP_OK == config_set_value(NVS_TYPE_STR, filename.c_str(), fileContent.c_str()));
	return true;
}

bool NVSFile::flush() {
	esp_err_t err = ESP_OK;

	for (auto it = files.begin(); it != files.end(); ++it) {
		err |= config_set_value(NVS_TYPE_STR, it->first.c_str(), it->second.c_str());
	}
	return (err == ESP_OK);
}

/****************************************************************************************
 * Shim HTTP server for spirc
 */
static esp_err_t handlerWrapper(httpd_req_t *req) {
	bell::HTTPRequest request = { };
	char *query = NULL, *body = NULL;
	bell::httpHandler *handler = (bell::httpHandler*) req->user_ctx;
	size_t query_len = httpd_req_get_url_query_len(req);

	request.connection = httpd_req_to_sockfd(req);

	// get body if any (add '\0' at the end if used as string)
	if (req->content_len) {
		body = (char*) calloc(1, req->content_len + 1);
		int size = httpd_req_recv(req, body, req->content_len);
		request.body = body;
		ESP_LOGD(TAG,"wrapper received body %d/%d", size, req->content_len);
	}

	// parse query if any (can be in body as well for url-encoded)
	if (query_len) {
		query = (char*) malloc(query_len + 1);
		httpd_req_get_url_query_str(req, query, query_len + 1);
	} else if (body && strchr(body, '&')) {
		query = body;
		body = NULL;
	}	
		
	// I know this is very crude and unsafe...
	url_decode(query);
	char *key = strtok(query, "&");

	while (key) {
		char *value = strchr(key, '=');
		*value++ = '\0';
		request.queryParams[key] = value;
		ESP_LOGD(TAG,"wrapper received key:%s value:%s", key, value);
		key = strtok(NULL, "&");
	};

	if (query) free(query);
	if (body) free(body);
	
	/*
	 This is a strange construct as the C++ handler will call the ShimHTTPSer::respond 
	 and then we'll return. So we can't obtain the response to be sent, as esp_http_server
	 normally expects, instead respond() will use raw socket and close connection
	*/
	(*handler)(request);

	return ESP_OK;
}

void ShimHTTPServer::registerHandler(bell::RequestType requestType, const std::string &routeUrl, bell::httpHandler handler) {
	httpd_uri_t request = { 
		.uri = routeUrl.c_str(), 
		.method = (requestType == bell::RequestType::GET ? HTTP_GET : HTTP_POST),
		.handler = handlerWrapper,
		.user_ctx = NULL,
	};

	// find athe first free spot and register handler
	for (int i = 0; i < sizeof(uriHandlers)/sizeof(bell::httpHandler); i++) {
		if (!uriHandlers[i]) {
			uriHandlers[i] = handler;
			request.user_ctx = uriHandlers + i;
			httpd_register_uri_handler(serverHandle, &request);
			break;	
		}
	}
		
	if (!request.user_ctx) ESP_LOGW(TAG, "Cannot add handler for %s", routeUrl.c_str());
}

void ShimHTTPServer::respond(const bell::HTTPResponse &response) {
	char *buf;
	size_t len = asprintf(&buf, "HTTP/1.1 %d OK\r\n"
			"Server: SQUEEZEESP32\r\n"
			"Connection: close\r\n"		
			"Content-type: %s\r\n"		
			"Content-length: %d\r\n"	
			"Access-Control-Allow-Origin: *\r\n"
			"Access-Control-Allow-Methods: GET, POST, PATCH, PUT, DELETE, OPTIONS\r\n"
			"Access-Control-Allow-Headers: Origin, Content-Type, X-Auth-Token\r\n"
			"\r\n%s", 
			response.status, response.contentType.c_str(), 
			response.body.size(), response.body.c_str()
	);

	// use raw socket send and close connection
	httpd_socket_send(serverHandle, response.connectionFd, buf, len, 0);
	free(buf);
	
	// we want to close the socket due to the strange construct
	httpd_sess_trigger_close(serverHandle, response.connectionFd);
}
