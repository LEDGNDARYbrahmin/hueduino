#ifndef HTTPS_SERVER_H
#define HTTPS_SERVER_H

#include <Arduino.h>
#include <functional>

class HTTPSServerWrapper {
public:
    using RequestCallback = std::function<String(String path, String method, String body)>;

    HTTPSServerWrapper(uint16_t port);
    ~HTTPSServerWrapper();

    bool begin(const char* cert, const char* key);
    void stop();
    void handleClient();
    void setRequestHandler(RequestCallback callback);

    // Static callback for the C-style ESP-IDF handler
    static RequestCallback _requestHandler;

private:
    uint16_t _port;
    bool _running;
    
    // Legacy pointers (unused in native mode but kept for compatibility if needed)
    void* _server;
    void* _cert;

    // Internal handler (unused in native mode)
    static void _handleRequest(void* req, void* res);
};

// Rename to match existing code usage
typedef HTTPSServerWrapper HTTPSServer;

#endif // HTTPS_SERVER_H