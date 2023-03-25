/*
 *  This software is released under the MIT License.
 *  https://opensource.org/licenses/MIT
 *
 */

#include <string>
#include <streambuf>
#include <Session.h>
#include <PlainConnection.h>
#include <memory>
#include <vector>
#include <iostream>
#include <inttypes.h>
#include <fstream>
#include <stdarg.h>
#include <ApResolve.h>

#include "MDNSService.h"
#include "SpircHandler.h"
#include "LoginBlob.h"
#include "CentralAudioBuffer.h"
#include "Logger.h"
#include "Utils.h"

#include "esp_http_server.h"
#include "cspot_private.h"
#include "cspot_sink.h"
#include "platform_config.h"
#include "tools.h"

//#include "time.h"

/*
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
*/

static const char *TAG = "cspot";

class cspotPlayer *player;

/****************************************************************************************
 * Chunk manager class (task)
 */

class chunkManager : public bell::Task {
public:
    std::atomic<bool> isRunning = true;
    std::atomic<bool> isPaused = true;
    chunkManager(std::shared_ptr<bell::CentralAudioBuffer> centralAudioBuffer, std::function<void()> trackHandler, std::function<void(const uint8_t*, size_t)> audioHandler);
    void teardown();

private:
    std::shared_ptr<bell::CentralAudioBuffer> centralAudioBuffer;
    std::function<void()> trackHandler;
    std::function<void(const uint8_t*, size_t)> audioHandler;
    std::mutex runningMutex;

    void runTask() override;
};

chunkManager::chunkManager(std::shared_ptr<bell::CentralAudioBuffer> centralAudioBuffer,
                            std::function<void()> trackHandler, std::function<void(const uint8_t*, size_t)> audioHandler)
    : bell::Task("player", 4 * 1024, 0, 0) {
    this->centralAudioBuffer = centralAudioBuffer;
    this->trackHandler = trackHandler;
    this->audioHandler = audioHandler;
    startTask();
}

void chunkManager::runTask() {
    std::scoped_lock lock(runningMutex);
    size_t lastHash = 0;

    while (isRunning) {

        if (isPaused) {
            BELL_SLEEP_MS(100);
            continue;
        }

        auto chunk = centralAudioBuffer->readChunk();

        if (!chunk || chunk->pcmSize == 0) {
            BELL_SLEEP_MS(50);
            continue;
        }

        // receiving first chunk of new track from Spotify server
        if (lastHash != chunk->trackHash) {
            CSPOT_LOG(info, "hash update %x => %x", lastHash, chunk->trackHash);
            lastHash = chunk->trackHash;
            trackHandler();
        }

        audioHandler(chunk->pcmData, chunk->pcmSize);
    }
}

void chunkManager::teardown() {
    isRunning = false;
    std::scoped_lock lock(runningMutex);
}

/****************************************************************************************
 * Player's main class  & task
 */

class cspotPlayer : public bell::Task {
private:
    std::string name;
    bool playback = false;
    bell::WrappedSemaphore clientConnected;
    std::shared_ptr<bell::CentralAudioBuffer> centralAudioBuffer;

    TimerHandle_t trackTimer;

    int startOffset, volume = 0, bitrate = 160;
    httpd_handle_t serverHandle;
    int serverPort;
    cspot_cmd_cb_t cmdHandler;
	cspot_data_cb_t dataHandler;

    std::shared_ptr<cspot::LoginBlob> blob;
    std::unique_ptr<cspot::SpircHandler> spirc;
    std::unique_ptr<chunkManager> chunker;

    void eventHandler(std::unique_ptr<cspot::SpircHandler::Event> event);
    void trackHandler(void);

    void runTask();

public:
    std::atomic<bool> trackNotify = false;

    cspotPlayer(const char*, httpd_handle_t, int, cspot_cmd_cb_t, cspot_data_cb_t);
    ~cspotPlayer();
    esp_err_t handleGET(httpd_req_t *request);
    esp_err_t handlePOST(httpd_req_t *request);
};

cspotPlayer::cspotPlayer(const char* name, httpd_handle_t server, int port, cspot_cmd_cb_t cmdHandler, cspot_data_cb_t dataHandler) :
                        bell::Task("playerInstance", 32 * 1024, 0, 0),
                        serverHandle(server), serverPort(port),
                        cmdHandler(cmdHandler), dataHandler(dataHandler) {
                            
    cJSON *item, *config = config_alloc_get_cjson("cspot_config");
    if ((item = cJSON_GetObjectItem(config, "volume")) != NULL) volume = item->valueint;
    if ((item = cJSON_GetObjectItem(config, "bitrate")) != NULL) bitrate = item->valueint;
    if ((item = cJSON_GetObjectItem(config, "deviceName") ) != NULL) this->name = item->valuestring;
    else this->name = name;
    cJSON_Delete(config);
    
    if (bitrate != 96 && bitrate != 160 && bitrate != 320) bitrate = 160;
}

cspotPlayer::~cspotPlayer() {
}

extern "C" {
    static esp_err_t handleGET(httpd_req_t *request) {
        return player->handleGET(request);
    }

    static esp_err_t handlePOST(httpd_req_t *request) {
        return player->handlePOST(request);
    }

    static void trackTimerHandler(TimerHandle_t xTimer) {
        player->trackNotify = true;
    }
}

esp_err_t cspotPlayer::handleGET(httpd_req_t *request) {
    std::string body = this->blob->buildZeroconfInfo();

    if (body.size() == 0) {
        CSPOT_LOG(info, "cspot empty blob's body on GET");
        return ESP_ERR_HTTPD_INVALID_REQ;
    }

    httpd_resp_set_hdr(request, "Content-type", "application/json");
    httpd_resp_set_hdr(request, "Content-length", std::to_string(body.size()).c_str());
    httpd_resp_send(request, body.c_str(), body.size());

    return ESP_OK;
}

esp_err_t cspotPlayer::handlePOST(httpd_req_t *request) {
   cJSON* response= cJSON_CreateObject();

   cJSON_AddNumberToObject(response, "status", 101);
   cJSON_AddStringToObject(response, "statusString", "ERROR-OK");
   cJSON_AddNumberToObject(response, "spotifyError", 0);
   
    // get body if any (add '\0' at the end if used as string)
	if (request->content_len) {
		char* body = (char*) calloc(1, request->content_len + 1);
		int size = httpd_req_recv(request, body, request->content_len);

        // I know this is very crude and unsafe...
        url_decode(body);
        char *key = strtok(body, "&");

        std::map<std::string, std::string> queryMap;

        while (key) {
            char *value = strchr(key, '=');
            *value++ = '\0';
            queryMap[key] = value;
            key = strtok(NULL, "&");
        };

        free(body);

        // Pass user's credentials to the blob and give the token
        blob->loadZeroconfQuery(queryMap);
        clientConnected.give();
    }

    char *responseStr = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);

    esp_err_t rc = httpd_resp_send(request, responseStr, strlen(responseStr));
    free(responseStr);
    
    return rc;
}

void cspotPlayer::eventHandler(std::unique_ptr<cspot::SpircHandler::Event> event) {
    switch (event->eventType) {
    case cspot::SpircHandler::EventType::PLAYBACK_START: {
        centralAudioBuffer->clearBuffer();

        // we are not playing anymore
        xTimerStop(trackTimer, portMAX_DELAY);
        trackNotify = false;
        playback = false;

        // memorize position for when track's beginning will be detected
        startOffset = std::get<int>(event->data);

        cmdHandler(CSPOT_START, 44100);
        CSPOT_LOG(info, "start track <%s>", spirc->getTrackPlayer()->getCurrentTrackInfo().name.c_str());

        // Spotify servers do not send volume at connection
        spirc->setRemoteVolume(volume);
        break;
    }
    case cspot::SpircHandler::EventType::PLAY_PAUSE: {
        bool pause = std::get<bool>(event->data);
        cmdHandler(pause ? CSPOT_PAUSE : CSPOT_PLAY);
        chunker->isPaused = pause;
        break;
    }
    case cspot::SpircHandler::EventType::TRACK_INFO: {
        auto trackInfo = std::get<cspot::CDNTrackStream::TrackInfo>(event->data);
        cmdHandler(CSPOT_TRACK, trackInfo.duration, startOffset, trackInfo.artist.c_str(),
                       trackInfo.album.c_str(), trackInfo.name.c_str(), trackInfo.imageUrl.c_str());
        spirc->updatePositionMs(startOffset);
        startOffset = 0;
        break;
    }
    case cspot::SpircHandler::EventType::NEXT:
    case cspot::SpircHandler::EventType::PREV:
    case cspot::SpircHandler::EventType::FLUSH: {
        // FLUSH is sent when there is no next, just clean everything
        centralAudioBuffer->clearBuffer();
        cmdHandler(CSPOT_FLUSH);
        break;
    }
    case cspot::SpircHandler::EventType::DISC:
        centralAudioBuffer->clearBuffer();
        xTimerStop(trackTimer, portMAX_DELAY);
        cmdHandler(CSPOT_DISC);
        chunker->teardown();
        break;
    case cspot::SpircHandler::EventType::SEEK: {
        centralAudioBuffer->clearBuffer();
        cmdHandler(CSPOT_SEEK, std::get<int>(event->data));
        break;
    }
    case cspot::SpircHandler::EventType::DEPLETED:
        CSPOT_LOG(info, "playlist ended, no track left to play");
        break;
    case cspot::SpircHandler::EventType::VOLUME:
        volume = std::get<int>(event->data);
        cmdHandler(CSPOT_VOLUME, volume);
        break;
    default:
        break;
    }
}

void cspotPlayer::trackHandler(void) {
    if (playback) {
        uint32_t remains;
        auto trackInfo = spirc->getTrackPlayer()->getCurrentTrackInfo();
        // if this is not first track, estimate when the current one will finish
        cmdHandler(CSPOT_REMAINING, &remains);
        if (remains > 100) xTimerChangePeriod(trackTimer, pdMS_TO_TICKS(remains), portMAX_DELAY);
        else trackNotify = true;
        CSPOT_LOG(info, "next track <%s> in cspot buffers, remaining %d ms", trackInfo.name.c_str(), remains);
    } else {
        trackNotify = true;
        playback = true;
    }  
}

void cspotPlayer::runTask() {
    httpd_uri_t request = {
		.uri = "/spotify_info",
		.method = HTTP_GET,
		.handler = ::handleGET,
		.user_ctx = NULL,
	};

    // register GET and POST handler for built-in server
    httpd_register_uri_handler(serverHandle, &request);
    request.method = HTTP_POST;
    request.handler = ::handlePOST;
    httpd_register_uri_handler(serverHandle, &request);

    // construct blob for that player
    blob = std::make_unique<cspot::LoginBlob>(name);

    // Register mdns service, for spotify to find us
    bell::MDNSService::registerService( blob->getDeviceName(), "_spotify-connect", "_tcp", "", serverPort,
            { {"VERSION", "1.0"}, {"CPath", "/spotify_info"}, {"Stack", "SP"} });
                            
                                static int count = 0;
    // gone with the wind...
    while (1) {
        clientConnected.wait();

        CSPOT_LOG(info, "Spotify client connected for %s", name.c_str());
        
        centralAudioBuffer = std::make_shared<bell::CentralAudioBuffer>(32);        
        auto ctx = cspot::Context::createFromBlob(blob);
             
        if (bitrate == 320) ctx->config.audioFormat = AudioFormat_OGG_VORBIS_320;
        else if (bitrate == 96) ctx->config.audioFormat = AudioFormat_OGG_VORBIS_96;
        else ctx->config.audioFormat = AudioFormat_OGG_VORBIS_160;            

        ctx->session->connectWithRandomAp();
        auto token = ctx->session->authenticate(blob);      

        // Auth successful
        if (token.size() > 0) {
            trackTimer = xTimerCreate("trackTimer", pdMS_TO_TICKS(1000), pdFALSE, NULL, trackTimerHandler);
            spirc = std::make_unique<cspot::SpircHandler>(ctx);

            // set call back to calculate a hash on trackId
            spirc->getTrackPlayer()->setDataCallback(
                [this](uint8_t* data, size_t bytes, std::string_view trackId, size_t sequence) {
                    return centralAudioBuffer->writePCM(data, bytes, sequence);
            });

            // set event (PLAY, VOLUME...) handler
            spirc->setEventHandler(
                [this](std::unique_ptr<cspot::SpircHandler::Event> event) {
                    eventHandler(std::move(event));
            });

            // Start handling mercury messages
            ctx->session->startTask();

            // Create a player, pass the tack handler
            chunker = std::make_unique<chunkManager>(centralAudioBuffer,
                [this](void) {
                    return trackHandler();
                },
                [this](const uint8_t* data, size_t bytes) {
                    return dataHandler(data, bytes);
             });

            // exit when player has stopped (received a DISC)
            while (chunker->isRunning) {
                ctx->session->handlePacket();

                // inform Spotify that next track has started (don't need to be super accurate)
                if (trackNotify) {
                    CSPOT_LOG(info, "next track's audio has reached DAC");
                    spirc->notifyAudioReachedPlayback();
                    trackNotify = false;
                }
            }

            xTimerDelete(trackTimer, portMAX_DELAY);
            spirc->disconnect();
                       
            CSPOT_LOG(info, "disconnecting player %s", name.c_str());
        }
        
        // we want to release memory ASAP and fore sure
        centralAudioBuffer.reset();
        ctx.reset();
        token.clear();

        // update volume when we disconnect
        cJSON *item, *config = config_alloc_get_cjson("cspot_config");
        cJSON_DeleteItemFromObject(config, "volume");
        cJSON_AddNumberToObject(config, "volume", volume);
        config_set_cjson_str_and_free("cspot_config", config);
    }
}

/****************************************************************************************
 * API to create and start a cspot instance
 */
 
struct cspot_s* cspot_create(const char *name, httpd_handle_t server, int port, cspot_cmd_cb_t cmd_cb, cspot_data_cb_t data_cb) {
	bell::setDefaultLogger();
    player = new cspotPlayer(name, server, port, cmd_cb, data_cb);
    player->startTask();
	return (cspot_s*) player;
}

/****************************************************************************************
 * Commands sent by local buttons/actions
 */
 
bool cspot_cmd(struct cspot_s* ctx, cspot_event_t event, void *param) {
	// we might have no controller left
/*
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
*/

	return true;
}
