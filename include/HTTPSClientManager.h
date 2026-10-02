#ifndef HTTPS_CLIENT_MANAGER_H
#define HTTPS_CLIENT_MANAGER_H

#include <ESP8266HTTPClient.h>
#include "HTTPSRedirect.h"

class HTTPSClientManager {
private:
    HTTPSRedirect* _client;
    int _port;

public:
    HTTPSClientManager(int port = 443) : _client(nullptr), _port(port) {}
    
    ~HTTPSClientManager() {
        if (_client) delete _client;
    }

    bool connect(const String& host) {
        if (!_client) {
            _client = new HTTPSRedirect(_port);
            _client->setInsecure();
        }
        
        int retry = 0;
        while (retry < 5) {
            if (_client->connect(host, _port) == 1) return true;
            retry++;
            delay(500);
        }
        return false;
    }

    String get(const String& url, const String& host) {
        if (!_client) return "";
        _client->GET(url, host.c_str());
        return _client->getResponseBody();
    }

    void disconnect() {
        if (_client) {}
    }
};

#endif
