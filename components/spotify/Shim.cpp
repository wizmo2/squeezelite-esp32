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

static class cspotPlayer *player;

/****************************************************************************************
 * Chunk manager class (task)
 */

class chunkManager : public bell::Task {
public:
    std::atomic<bool> isRunning = true;
    std::atomic<bool> isPaused = true;
    chunkManager(std::shared_ptr<bell::CentralAudioBuffer> centralAudioBuffer, std::function<void()> trackHandler,
                 std::function<void(const uint8_t*, size_t)> dataHandler);
    void teardown();

private:
    std::shared_ptr<bell::CentralAudioBuffer> centralAudioBuffer;
    std::function<void()> trackHandler;
    std::function<void(const uint8_t*, size_t)> dataHandler;
    std::mutex runningMutex;

    void runTask() override;
};

chunkManager::chunkManager(std::shared_ptr<bell::CentralAudioBuffer> centralAudioBuffer,
                            std::function<void()> trackHandler, std::function<void(const uint8_t*, size_t)> dataHandler)
    : bell::Task("chunker", 4 * 1024, 0, 0) {
    this->centralAudioBuffer = centralAudioBuffer;
    this->trackHandler = trackHandler;
    this->dataHandler = dataHandler;
    startTask();
}

void chunkManager::teardown() {
    isRunning = false;
    std::scoped_lock lock(runningMutex);
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

        dataHandler(chunk->pcmData, chunk->pcmSize);
    }
}

/****************************************************************************************
 * Player's main class  & task
 */

class cspotPlayer : public bell::Task {
private:
    std::string name;
    bell::WrappedSemaphore clientConnected;
    std::shared_ptr<bell::CentralAudioBuffer> centralAudioBuffer;

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
        trackStatus = TRACK_INIT;
        // memorize position for when track's beginning will be detected
        startOffset = std::get<int>(event->data);
        // Spotify servers do not send volume at connection
        spirc->setRemoteVolume(volume);

        cmdHandler(CSPOT_START, 44100);
        CSPOT_LOG(info, "restart");
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
        centralAudioBuffer->clearBuffer();
        cmdHandler(CSPOT_FLUSH);
        break;
    }
    case cspot::SpircHandler::EventType::DISC:
        centralAudioBuffer->clearBuffer();
        cmdHandler(CSPOT_DISC);
        chunker->teardown();
        break;
    case cspot::SpircHandler::EventType::SEEK: {
        centralAudioBuffer->clearBuffer();
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
    auto trackInfo = spirc->getTrackPlayer()->getCurrentTrackInfo();
    uint32_t remains;
    cmdHandler(CSPOT_QUERY_REMAINING, &remains);
    CSPOT_LOG(info, "next track <%s> will play in %d ms", trackInfo.name.c_str(), remains);

    // inform sink of track beginning
    trackStatus = TRACK_NOTIFY;
    cmdHandler(CSPOT_TRACK_MARK);
}

void cspotPlayer::command(cspot_event_t event) {
    if (!spirc) return;

    // switch...case consume a ton of extra .rodata
    if (event == CSPOT_PREV) spirc->previousSong();
    else if (event == CSPOT_NEXT) spirc->nextSong();
    else if (event == CSPOT_TOGGLE)	spirc->setPause(!chunker->isPaused);
    else if (event == CSPOT_STOP || event == CSPOT_PAUSE) spirc->setPause(true);
    else if (event == CSPOT_PLAY) spirc->setPause(false);
    else if (event == CSPOT_DISC) spirc->disconnect();
    else if (event == CSPOT_VOLUME_UP) {
        volume += (UINT16_MAX / 50);
        volume = std::min(volume, UINT16_MAX);
        cmdHandler(CSPOT_VOLUME, volume);
        spirc->setRemoteVolume(volume);
    } else if (event == CSPOT_VOLUME_DOWN) {
        volume -= (UINT16_MAX / 50);
        volume = std::max(volume, 0);
        cmdHandler(CSPOT_VOLUME, volume);
        spirc->setRemoteVolume(volume);
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
             
             // set volume at connection
             cmdHandler(CSPOT_VOLUME, volume);

            // exit when player has stopped (received a DISC)
            while (chunker->isRunning) {
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

        // we want to release memory ASAP and fore sure
        centralAudioBuffer.reset();
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
