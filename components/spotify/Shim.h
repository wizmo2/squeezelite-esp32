/* 
 *  This software is released under the MIT License.
 *  https://opensource.org/licenses/MIT
 *
 */

#pragma once

#include <vector>
#include <iostream>
#include <map>
#include "AudioSink.h"
#include "FileHelper.h"
#include "BaseHTTPServer.h"
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"

class ShimAudioSink : public AudioSink {
public:
	ShimAudioSink(void) { softwareVolumeControl = false; }
    void feedPCMFrames(const uint8_t *data, size_t bytes);
	virtual void volumeChanged(uint16_t volume);
};

class NVSFile : public FileHelper {
private:
	std::map<std::string, std::string> files;

public:
    bool readFile(std::string filename, std::string &fileContent);
    bool writeFile(std::string filename, std::string fileContent);
	bool flush();
};

class ShimHTTPServer : public bell::BaseHTTPServer {    
private:
	httpd_handle_t serverHandle;
	bell::httpHandler uriHandlers[4];
	
public:
   ShimHTTPServer(httpd_handle_t server, int port) { serverHandle = server; serverPort = port; }

   void registerHandler(bell::RequestType requestType, const std::string &, bell::httpHandler);
   void respond(const bell::HTTPResponse &);
};
