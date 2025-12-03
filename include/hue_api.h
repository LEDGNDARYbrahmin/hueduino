#ifndef HUE_API_H
#define HUE_API_H

#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "config.h"

// Hue API v1 Handler (Synchronous WebServer)
class HueAPI {
public:
    HueAPI(WebServer* server);
    void begin();
    
    // Main dispatcher for dynamic /api/... routes
    void handleApiRequest();
    
    // Get description.xml content
    String getDescriptionXML();
    String generateUsername();

private:
    WebServer* _server;
    
    // Specific route handlers
    void handleDescription();
    void handleCreateUser();
    void handleGetConfig();
    
    // Dynamic route handlers
    void handleFullState(const String& username);
    void handleGetLights(const String& username);
    void handleGetLight(const String& username, int lightId);
    void handleSetLightState(const String& username, int lightId);
    void handleGetGroups(const String& username);
    void handleGetGroup(const String& username, int groupId);
    void handleSetGroupState(const String& username, int groupId);
    
    // Helpers
    bool validateApiKey(const String& username);
    void sendError(int errorCode, const String& description);
    String buildLightJson(uint8_t lightId);
    String buildGroupJson(uint8_t groupId);
    String buildConfigJson(bool full);
};

// Light control functions
bool setLightState(uint8_t lightId, JsonObject& state);
bool setGroupState(uint8_t groupId, JsonObject& state);
bool sendLightCommand(uint8_t lightId, JsonObject& state);

// Custom Request Handler to silence "handler not found" errors
class HueRequestHandler : public RequestHandler {
public:
    HueRequestHandler(HueAPI* api) : _api(api) {}

    bool canHandle(HTTPMethod method, String uri) override {
        return uri.startsWith("/api");
    }

    bool handle(WebServer& server, HTTPMethod requestMethod, String requestUri) override {
        _api->handleApiRequest();
        return true;
    }

private:
    HueAPI* _api;
};

#endif // HUE_API_H
