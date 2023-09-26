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

#include "BellTask.h"
#include "MDNSService.h"
#include "TrackPlayer.h"
#include "CSpotContext.h"
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

static class cspotPlayer *player;

/****************************************************************************************
 * Player's main class  & task
 */

class cspotPlayer : public bell::Task {
private:
    std::string name;
    bell::WrappedSemaphore clientConnected;
    std::atomic<bool> isPaused, isConnected;
        
    int startOffset, volume = 0, bitrate = 160;
    httpd_handle_t serverHandle;
    int serverPort;
    cspot_cmd_cb_t cmdHandler;
    cspot_data_cb_t dataHandler;
    std::string lastTrackId;

    std::shared_ptr<cspot::LoginBlob> blob;
    std::unique_ptr<cspot::SpircHandler> spirc;

    void eventHandler(std::unique_ptr<cspot::SpircHandler::Event> event);
    void trackHandler(void);
    size_t pcmWrite(uint8_t *pcm, size_t bytes, std::string_view trackId);

    void runTask();

public:
    typedef enum {TRACK_INIT, TRACK_NOTIFY, TRACK_STREAM, TRACK_END} TrackStatus;
    std::atomic<TrackStatus> trackStatus = TRACK_INIT;

    cspotPlayer(const char*, httpd_handle_t, int, cspot_cmd_cb_t, cspot_data_cb_t);
    esp_err_t handleGET(httpd_req_t *request);
    esp_err_t handlePOST(httpd_req_t *request);
    void command(cspot_event_t event);
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

size_t cspotPlayer::pcmWrite(uint8_t *pcm, size_t bytes, std::string_view trackId) {
    if (lastTrackId != trackId) {
        CSPOT_LOG(info, "new track started <%s> => <%s>", lastTrackId.c_str(), trackId.data());
        lastTrackId = trackId;
        trackHandler();
    }

    return dataHandler(pcm, bytes);
}    

extern "C" {
    static esp_err_t handleGET(httpd_req_t *request) {
        return player->handleGET(request);
    }

    static esp_err_t handlePOST(httpd_req_t *request) {
        return player->handlePOST(request);
    }
}

esp_err_t cspotPlayer::handleGET(httpd_req_t *request) {
    std::string body = this->blob->buildZeroconfInfo();

    if (body.size() == 0) {
        CSPOT_LOG(info, "cspot empty blob's body on GET");
        return ESP_ERR_HTTPD_INVALID_REQ;
    }

    httpd_resp_set_hdr(request, "Content-type", "application/json");
    httpd_resp_send(request, body.c_str(), body.size());

    return ESP_OK;
}

esp_err_t cspotPlayer::handlePOST(httpd_req_t *request) {
    cJSON* response= cJSON_CreateObject(); 
    //see https://developer.spotify.com/documentation/commercial-hardware/implementation/guides/zeroconf

    if (cmdHandler(CSPOT_BUSY)) {
        cJSON_AddNumberToObject(response, "status", 101);
        cJSON_AddStringToObject(response, "statusString", "OK");
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
    } else {
        cJSON_AddNumberToObject(response, "status", 202);
        cJSON_AddStringToObject(response, "statusString", "ERROR-LOGIN-FAILED");
        cJSON_AddNumberToObject(response, "spotifyError", 0);
        
        CSPOT_LOG(info, "sink is busy, can't accept request");
    }

    char *responseStr = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);
    
    httpd_resp_set_hdr(request, "Content-type", "application/json");
    esp_err_t rc = httpd_resp_send(request, responseStr, strlen(responseStr));
    free(responseStr);

    return rc;
}

void cspotPlayer::eventHandler(std::unique_ptr<cspot::SpircHandler::Event> event) {
    switch (event->eventType) {
    case cspot::SpircHandler::EventType::PLAYBACK_START: {
        lastTrackId.clear();
        // we are not playing anymore
        trackStatus = TRACK_INIT;
        // memorize position for when track's beginning will be detected
        startOffset = std::get<int>(event->data);
        // Spotify servers do not send volume at connection
        spirc->setRemoteVolume(volume);

        cmdHandler(CSPOT_START, 44100);
        CSPOT_LOG(info, "(re)start playing");
        break;
    }
    case cspot::SpircHandler::EventType::PLAY_PAUSE: {
        isPaused = std::get<bool>(event->data);
        cmdHandler(isPaused ? CSPOT_PAUSE : CSPOT_PLAY);
        break;
    }
    case cspot::SpircHandler::EventType::TRACK_INFO: {
        auto trackInfo = std::get<cspot::TrackInfo>(event->data);
        cmdHandler(CSPOT_TRACK_INFO, trackInfo.duration, startOffset, trackInfo.artist.c_str(),
                       trackInfo.album.c_str(), trackInfo.name.c_str(), trackInfo.imageUrl.c_str());
        spirc->updatePositionMs(startOffset);
        startOffset = 0;
        break;
    }
    case cspot::SpircHandler::EventType::NEXT:
    case cspot::SpircHandler::EventType::PREV:
    case cspot::SpircHandler::EventType::FLUSH: {
        // FLUSH is sent when there is no next, just clean everything
        cmdHandler(CSPOT_FLUSH);
        break;
    }
    case cspot::SpircHandler::EventType::DISC:
        cmdHandler(CSPOT_DISC);
        isConnected = false;
        break;
    case cspot::SpircHandler::EventType::SEEK: {
        cmdHandler(CSPOT_SEEK, std::get<int>(event->data));
        break;
    }
    case cspot::SpircHandler::EventType::DEPLETED:
        trackStatus = TRACK_END;
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
    // this is just informative
    uint32_t remains;
    cmdHandler(CSPOT_QUERY_REMAINING, &remains);
    CSPOT_LOG(info, "next track will play in %d ms", remains);

    // inform sink of track beginning
    trackStatus = TRACK_NOTIFY;
    cmdHandler(CSPOT_TRACK_MARK);
}

void cspotPlayer::command(cspot_event_t event) {
    if (!spirc) return;

    // switch...case consume a ton of extra .rodata
    switch (event) {
    // nextSong/previousSong come back through cspot::event as a FLUSH
    case CSPOT_PREV:
        spirc->previousSong();
        break;
    case CSPOT_NEXT:
        spirc->nextSong();
        break;
    // setPause comes back through cspot::event with PLAY/PAUSE
    case CSPOT_TOGGLE:
        isPaused = !isPaused;
        spirc->setPause(isPaused);
        break;
    case CSPOT_STOP:
    case CSPOT_PAUSE:
        spirc->setPause(true);
        break;
    case CSPOT_PLAY:
        spirc->setPause(false);
        break;
    /* Calling spirc->disconnect() might have been logical but it does not
     * generate any cspot::event */
    case CSPOT_DISC:
        cmdHandler(CSPOT_DISC);
        isConnected = false;
        break;
    // spirc->setRemoteVolume does not generate a cspot::event so call cmdHandler
    case CSPOT_VOLUME_UP:
        volume += (UINT16_MAX / 50);
        volume = std::min(volume, UINT16_MAX);
        cmdHandler(CSPOT_VOLUME, volume);
        spirc->setRemoteVolume(volume);
        break;
    case CSPOT_VOLUME_DOWN:
        volume -= (UINT16_MAX / 50);
        volume = std::max(volume, 0);
        cmdHandler(CSPOT_VOLUME, volume);
        spirc->setRemoteVolume(volume);
        break;
    default:
        break;
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
            
    CSPOT_LOG(info, "CSpot instance service name %s (id %s)", blob->getDeviceName().c_str(), blob->getDeviceId().c_str());

    // gone with the wind...
    while (1) {
        clientConnected.wait();

        CSPOT_LOG(info, "Spotify client connected for %s", name.c_str());

        auto ctx = cspot::Context::createFromBlob(blob);

        if (bitrate == 320) ctx->config.audioFormat = AudioFormat_OGG_VORBIS_320;
        else if (bitrate == 96) ctx->config.audioFormat = AudioFormat_OGG_VORBIS_96;
        else ctx->config.audioFormat = AudioFormat_OGG_VORBIS_160;

        ctx->session->connectWithRandomAp();
        auto token = ctx->session->authenticate(blob);

        // Auth successful
        if (token.size() > 0) {
            spirc = std::make_unique<cspot::SpircHandler>(ctx);
            isConnected = true;            
			
            // set call back to calculate a hash on trackId
            spirc->getTrackPlayer()->setDataCallback(
                [this](uint8_t* data, size_t bytes, std::string_view trackId) {
                    return pcmWrite(data, bytes, trackId);
            });

            // set event (PLAY, VOLUME...) handler
            spirc->setEventHandler(
                [this](std::unique_ptr<cspot::SpircHandler::Event> event) {
                    eventHandler(std::move(event));
            });

            // Start handling mercury messages
            ctx->session->startTask();

            // set volume at connection
            cmdHandler(CSPOT_VOLUME, volume);

            // exit when player has stopped (received a DISC)
            while (isConnected) {
                ctx->session->handlePacket();

                // low-accuracy polling events
                if (trackStatus == TRACK_NOTIFY) {
                    // inform Spotify that next track has started (don't need to be super accurate)
                    uint32_t started;
                    cmdHandler(CSPOT_QUERY_STARTED, &started);
                    if (started) {
                        CSPOT_LOG(info, "next track's audio has reached DAC");
                        spirc->notifyAudioReachedPlayback();
                        trackStatus = TRACK_STREAM;
                    }
                } else if (trackStatus == TRACK_END) {
                    // wait for end of last track
                    uint32_t remains;
                    cmdHandler(CSPOT_QUERY_REMAINING, &remains);
                    if (!remains) {
                        CSPOT_LOG(info, "last track finished");
                        trackStatus = TRACK_INIT;
                        cmdHandler(CSPOT_STOP);
                        spirc->setPause(true);
                    }
                }
            }

            spirc->disconnect();
            spirc.reset();

            CSPOT_LOG(info, "disconnecting player %s", name.c_str());
        }

        // we want to release memory ASAP and for sure
        ctx.reset();
        token.clear();
        
        // update volume when we disconnect
        cJSON *config = config_alloc_get_cjson("cspot_config");
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
    player->command(event);
	return true;
}
