#include "URLParser.h"

namespace bell {

#ifdef BELL_DISABLE_REGEX
void URLParser::parse(const char* url, std::vector<std::string>& match) {
    match[0] = url;
    char scratch[512];
    
    // get schema [http(s)]://
    if (sscanf(url, "%[^:]:/", scratch) > 0) match[1] = scratch;
    
    // get host  http(s)://[host]
    if (sscanf(url, "htt%*[^:]://%512[^/#]", scratch) > 0) match[2] = scratch;

    // get the path
    url = strstr(url, match[2].c_str());
    if (!url || *url == '\0') return;
    url += match[2].size();
    if (sscanf(url, "/%512[^?]", scratch) > 0) match[3] = scratch;
    
    // get the query
    if (match[3].size()) url += match[3].size() + 1;
    if (sscanf(url, "?%512[^#]", scratch) > 0) match[4] = scratch;

    // get the hash
    if (match[4].size()) url += match[4].size() + 1;
    if (sscanf(url, "#%512s", scratch) > 0) match[5] = scratch;

    // fix the acquired items
    match[3] = "/" + match[3];
    if (match[4].size()) match[4] = "?" + match[4];
}    
#else    
const std::regex URLParser::urlParseRegex = std::regex(
      "^(?:([^:/?#]+):)?(?://([^/?#]*))?([^?#]*)(\\?(?:[^#]*))?(#(?:.*))?");
#endif
} 
