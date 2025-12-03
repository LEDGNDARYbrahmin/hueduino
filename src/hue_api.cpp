#include "hue_api.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>

HueAPI::HueAPI(WebServer* server) : _server(server) {}

void HueAPI::begin() {
    Serial.println("[HUE API] Initializing Hue API v1 (Synchronous)...");
    
    // Static routes
    _server->on("/description.xml", HTTP_GET, [this]() {
        handleDescription();
    });
    
    // Explicit handlers for pairing endpoints
    _server->on("/api", HTTP_POST, [this]() {
        Serial.println("[HUE API] POST /api called (Pairing Request)");
        handleCreateUser();
    });
    
   // _server->on("/api/", HTTP_POST, [this]() {
   //     Serial.println("[HUE API] POST /api/ called (Pairing Request)");
   //     handleCreateUser();
   // });
}

void HueAPI::handleApiRequest() {
    String uri = _server->uri();
    HTTPMethod method = _server->method();
    
    // 1. /api (POST) -> Create User (handle both /api and /api/)
    if ((uri == "/api" || uri == "/api/") && method == HTTP_POST) {
        handleCreateUser();
        return;
    }
    
    // 2. /api/config (GET)
    if (uri == "/api/config" && method == HTTP_GET) {
        handleGetConfig();
        return;
    }
    
    // 3. Dynamic routes: /api/<username>/...
    if (uri.startsWith("/api/")) {
        // Extract username
        // uri is like "/api/username/resource..."
        int uStart = 5; // length of "/api/"
        int uEnd = uri.indexOf('/', uStart);
        
        String username;
        String remainder;
        
        if (uEnd == -1) {
            // /api/username
            username = uri.substring(uStart);
            remainder = "";
        } else {
            username = uri.substring(uStart, uEnd);
            remainder = uri.substring(uEnd + 1); // "lights", "lights/1", etc.
        }
        
        if (username.length() == 0) {
            _server->send(404, "text/plain", "Not found");
            return;
        }
        
        // /api/<username> (Full State)
        if (remainder.length() == 0 && method == HTTP_GET) {
            handleFullState(username);
            return;
        }
        
        // /api/<username>/config
        if (remainder == "config" && method == HTTP_GET) {
            handleGetConfig();
            return;
        }
        
        // Lights
        if (remainder.startsWith("lights")) {
            // remainder is "lights" or "lights/1" or "lights/1/state"
            String rSub = remainder.substring(6); // remove "lights"
            
            if (rSub.length() == 0 && method == HTTP_GET) {
                handleGetLights(username); // /api/<user>/lights
                return;
            }
            
            if (rSub.startsWith("/")) {
                String idStr = rSub.substring(1); // "1" or "1/state"
                int slash = idStr.indexOf('/');
                
                if (slash == -1) {
                    // /api/<user>/lights/<id>
                    if (method == HTTP_GET) {
                        handleGetLight(username, idStr.toInt());
                        return;
                    }
                } else {
                    // /api/<user>/lights/<id>/state
                    String idPart = idStr.substring(0, slash);
                    String action = idStr.substring(slash + 1);
                    
                    if (action == "state" && method == HTTP_PUT) {
                        handleSetLightState(username, idPart.toInt());
                        return;
                    }
                }
            }
        }
        
        // Groups
        if (remainder.startsWith("groups")) {
            String rSub = remainder.substring(6);
            
            if (rSub.length() == 0 && method == HTTP_GET) {
                handleGetGroups(username);
                return;
            }
            
            if (rSub.startsWith("/")) {
                String idStr = rSub.substring(1);
                int slash = idStr.indexOf('/');
                
                if (slash == -1) {
                    // /api/<user>/groups/<id>
                    if (method == HTTP_GET) {
                        handleGetGroup(username, idStr.toInt());
                        return;
                    }
                } else {
                    // /api/<user>/groups/<id>/action
                    String idPart = idStr.substring(0, slash);
                    String action = idStr.substring(slash + 1);
                    
                    if (action == "action" && method == HTTP_PUT) {
                        handleSetGroupState(username, idPart.toInt());
                        return;
                    }
                }
            }
        }
    }
    
    // If we got here, it's a 404 (unless main.cpp has other handlers)
    _server->send(404, "text/plain", "Not found");
}

void HueAPI::handleDescription() {
    _server->send(200, "text/xml", getDescriptionXML());
}

String HueAPI::getDescriptionXML() {
    IPAddress ip = WiFi.localIP();
    String xml = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n";
    xml += "<root xmlns=\"urn:schemas-upnp-org:device-1-0\">\n";
    xml += "<specVersion><major>1</major><minor>0</minor></specVersion>\n";
    xml += "<URLBase>http://" + ip.toString() + ":80/</URLBase>\n";
    xml += "<device>\n";
    xml += "<deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType>\n";
    xml += "<friendlyName>" + String(bridgeConfig.name) + "</friendlyName>\n";
    xml += "<manufacturer>" + String(bridgeConfig.manufacturer) + "</manufacturer>\n";
    xml += "<manufacturerURL>http://www.philips.com</manufacturerURL>\n";
    xml += "<modelDescription>Philips hue Personal Wireless Lighting</modelDescription>\n";
    xml += "<modelName>Philips hue bridge 2015</modelName>\n";
    xml += "<modelNumber>" + String(bridgeConfig.modelid) + "</modelNumber>\n";
    xml += "<modelURL>http://www.meethue.com</modelURL>\n";
    xml += "<serialNumber>" + String(bridgeConfig.bridgeid) + "</serialNumber>\n";
    xml += "<UDN>uuid:2f402f80-da50-11e1-9b23-" + String(bridgeConfig.bridgeid) + "</UDN>\n";
    xml += "</device>\n";
    xml += "</root>\n";
    return xml;
}

void HueAPI::handleCreateUser() {
    if (!_server->hasArg("plain")) {
        sendError(2, "body contains invalid JSON");
        return;
    }
    
    String body = _server->arg("plain");
    Serial.printf("[HUE API] Pairing body: %s\n", body.c_str());
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        Serial.printf("[HUE API] JSON deserialization error: %s\n", error.c_str());
        sendError(2, "body contains invalid JSON");
        return;
    }
    
    // Check link button
    // We rely on the main loop to clear bridgeConfig.linkbutton after timeout
    if (!bridgeConfig.linkbutton) {
        Serial.println("[HUE API] Pairing failed: Link button not pressed");
        DynamicJsonDocument response(256);
        JsonObject error = response.createNestedObject();
        error["type"] = 101;
        error["address"] = "/";
        error["description"] = "link button not pressed";
        
        JsonArray array = response.to<JsonArray>();
        array.add(error);
        
        String output;
        serializeJson(array, output);
        _server->send(200, "application/json", output);
        return;
    }
    
    if (apiUsersCount >= MAX_API_USERS) {
        Serial.printf("[HUE API] Pairing failed: Max users reached (%d)\n", apiUsersCount);
        sendError(301, "too many users");
        return;
    }
    
    Serial.println("[HUE API] Generating API key...");
    String username = generateApiKey();
    Serial.printf("[HUE API] Generated key: %s\n", username.c_str());
    
    const char* devicetype = doc["devicetype"] | "unknown#device";
    Serial.printf("[HUE API] Device type: %s\n", devicetype);
    
    ApiUser& user = apiUsers[apiUsersCount];
    strncpy(user.username, username.c_str(), sizeof(user.username) - 1);
    strncpy(user.devicetype, devicetype, sizeof(user.devicetype) - 1);
    
    // Time
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char timeStr[32];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%dT%H:%M:%S", &timeinfo);
    strncpy(user.create_date, timeStr, sizeof(user.create_date) - 1);
    strncpy(user.last_use_date, timeStr, sizeof(user.last_use_date) - 1);
    
    apiUsersCount++;
    Serial.println("[HUE API] Saving user to config...");
    saveConfig();
    Serial.println("[HUE API] User saved.");
    
    // Fix: Correctly structure the success response as [{"success": {"username": "..."}}]
    DynamicJsonDocument responseDoc(256);
    JsonArray array = responseDoc.to<JsonArray>();
    JsonObject successItem = array.createNestedObject();
    JsonObject successContent = successItem.createNestedObject("success");
    successContent["username"] = username;
    
    String output;
    serializeJson(array, output);
    Serial.printf("[HUE API] Created new user: %s (%s)\n", username.c_str(), devicetype);
    Serial.printf("[HUE API] Sending response: %s\n", output.c_str());
    _server->send(200, "application/json", output);
}

void HueAPI::handleGetConfig() {
    Serial.println("[HUE API] handleGetConfig() called");
    String json = buildConfigJson(false);
    Serial.printf("[HUE API] Sending config: %s\n", json.c_str());
    _server->send(200, "application/json", json);
}

void HueAPI::handleFullState(const String& username) {
    if (!validateApiKey(username)) {
        sendError(1, "unauthorized user");
        return;
    }
    
    DynamicJsonDocument doc(8192);
    
    // Lights
    JsonObject lightsObj = doc.createNestedObject("lights");
    for (uint8_t i = 0; i < lightsCount; i++) {
        String lightJson = buildLightJson(i);
        StaticJsonDocument<1024> lightDoc;
        deserializeJson(lightDoc, lightJson);
        lightsObj[String(i + 1)] = lightDoc.as<JsonObject>();
    }
    
    // Groups
    JsonObject groupsObj = doc.createNestedObject("groups");
    for (uint8_t i = 0; i < groupsCount; i++) {
        String groupJson = buildGroupJson(i);
        StaticJsonDocument<512> groupDoc;
        deserializeJson(groupDoc, groupJson);
        groupsObj[String(i)] = groupDoc.as<JsonObject>();
    }
    
    // Config
    String configJson = buildConfigJson(true);
    StaticJsonDocument<2048> configDoc;
    deserializeJson(configDoc, configJson);
    doc["config"] = configDoc.as<JsonObject>();
    
    doc.createNestedObject("scenes");
    doc.createNestedObject("schedules");
    doc.createNestedObject("sensors");
    doc.createNestedObject("rules");
    
    String output;
    serializeJson(doc, output);
    _server->send(200, "application/json", output);
}

void HueAPI::handleGetLights(const String& username) {
    Serial.printf("[HUE API] handleGetLights() called for user: %s\n", username.c_str());
    
    if (!validateApiKey(username)) {
        Serial.printf("[HUE API] API key validation FAILED for: %s\n", username.c_str());
        sendError(1, "unauthorized user");
        return;
    }
    
    Serial.printf("[HUE API] Returning %d lights\n", lightsCount);
    
    DynamicJsonDocument doc(4096);
    for (uint8_t i = 0; i < lightsCount; i++) {
        String lightJson = buildLightJson(i);
        StaticJsonDocument<1024> lightDoc;
        deserializeJson(lightDoc, lightJson);
        doc[String(i + 1)] = lightDoc.as<JsonObject>();
    }
    
    String output;
    serializeJson(doc, output);
    _server->send(200, "application/json", output);
}

void HueAPI::handleGetLight(const String& username, int lightId) {
    if (!validateApiKey(username)) {
        sendError(1, "unauthorized user");
        return;
    }
    
    int idx = lightId - 1;
    if (idx < 0 || idx >= lightsCount) {
        sendError(3, "resource not available");
        return;
    }
    
    String json = buildLightJson(idx);
    _server->send(200, "application/json", json);
}

void HueAPI::handleSetLightState(const String& username, int lightId) {
    if (!validateApiKey(username)) {
        sendError(1, "unauthorized user");
        return;
    }
    
    int idx = lightId - 1;
    if (idx < 0 || idx >= lightsCount) {
        sendError(3, "resource not available");
        return;
    }
    
    if (!_server->hasArg("plain")) {
        sendError(2, "body contains invalid JSON");
        return;
    }
    
    String body = _server->arg("plain");
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        sendError(2, "body contains invalid JSON");
        return;
    }
    
    JsonObject state = doc.as<JsonObject>();
    if (setLightState(idx, state)) {
        DynamicJsonDocument response(512);
        JsonArray array = response.to<JsonArray>();
        
        for (JsonPair kv : state) {
            JsonObject success = array.createNestedObject();
            String address = "/lights/" + String(lightId) + "/state/" + String(kv.key().c_str());
            success["success"][address] = kv.value();
        }
        
        String output;
        serializeJson(array, output);
        _server->send(200, "application/json", output);
    } else {
        sendError(201, "light is not reachable");
    }
}

void HueAPI::handleGetGroups(const String& username) {
    if (!validateApiKey(username)) {
        sendError(1, "unauthorized user");
        return;
    }
    
    DynamicJsonDocument doc(2048);
    for (uint8_t i = 0; i < groupsCount; i++) {
        String groupJson = buildGroupJson(i);
        StaticJsonDocument<512> groupDoc;
        deserializeJson(groupDoc, groupJson);
        doc[String(i)] = groupDoc.as<JsonObject>();
    }
    String output;
    serializeJson(doc, output);
    _server->send(200, "application/json", output);
}

void HueAPI::handleGetGroup(const String& username, int groupId) {
    if (!validateApiKey(username)) {
        sendError(1, "unauthorized user");
        return;
    }
    
    if (groupId >= groupsCount) {
        sendError(3, "resource not available");
        return;
    }
    
    String json = buildGroupJson(groupId);
    _server->send(200, "application/json", json);
}

void HueAPI::handleSetGroupState(const String& username, int groupId) {
    if (!validateApiKey(username)) {
        sendError(1, "unauthorized user");
        return;
    }
    
    if (groupId >= groupsCount) {
        sendError(3, "resource not available");
        return;
    }
    
    if (!_server->hasArg("plain")) {
        sendError(2, "body contains invalid JSON");
        return;
    }
    
    String body = _server->arg("plain");
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        sendError(2, "body contains invalid JSON");
        return;
    }
    
    JsonObject state = doc.as<JsonObject>();
    if (setGroupState(groupId, state)) {
        DynamicJsonDocument response(512);
        JsonArray array = response.to<JsonArray>();
        
        for (JsonPair kv : state) {
            JsonObject success = array.createNestedObject();
            String address = "/groups/" + String(groupId) + "/action/" + String(kv.key().c_str());
            success["success"][address] = kv.value();
        }
        
        String output;
        serializeJson(array, output);
        _server->send(200, "application/json", output);
    } else {
        sendError(201, "group action failed");
    }
}

bool HueAPI::validateApiKey(const String& username) {
    // Serial.printf("[HUE API] Validating key: %s against %d users\n", username.c_str(), apiUsersCount);
    for (uint8_t i = 0; i < apiUsersCount; i++) {
        if (username == apiUsers[i].username) {
            return true;
        }
    }
    return false;
}

void HueAPI::sendError(int errorCode, const String& description) {
    DynamicJsonDocument doc(256);
    JsonArray array = doc.to<JsonArray>();
    JsonObject error = array.createNestedObject();
    error["error"]["type"] = errorCode;
    error["error"]["address"] = _server->uri();
    error["error"]["description"] = description;
    
    String output;
    serializeJson(array, output);
    _server->send(200, "application/json", output);
}

String HueAPI::buildLightJson(uint8_t lightId) {
    Light& light = lights[lightId];
    DynamicJsonDocument doc(1024);
    
    JsonObject state = doc.createNestedObject("state");
    state["on"] = light.on;
    state["bri"] = light.bri;
    state["hue"] = light.hue;
    state["sat"] = light.sat;
    state["ct"] = light.ct;
    JsonArray xy = state.createNestedArray("xy");
    xy.add(light.xy[0]);
    xy.add(light.xy[1]);
    state["alert"] = "none";
    state["effect"] = "none";
    state["colormode"] = light.colormode == 1 ? "xy" : (light.colormode == 2 ? "ct" : "hs");
    state["reachable"] = light.reachable;
    
    doc["type"] = light.type;
    doc["name"] = light.name;
    doc["modelid"] = light.modelid;
    doc["manufacturername"] = light.manufacturername;
    doc["uniqueid"] = light.uniqueid;
    doc["swversion"] = light.swversion;
    
    String output;
    serializeJson(doc, output);
    return output;
}

String HueAPI::buildGroupJson(uint8_t groupId) {
    Group& group = groups[groupId];
    DynamicJsonDocument doc(512);
    
    doc["name"] = group.name;
    doc["type"] = group.type;
    
    JsonArray lightsArray = doc.createNestedArray("lights");
    for (uint8_t i = 0; i < group.lights_count; i++) {
        lightsArray.add(String(group.lights[i] + 1));
    }
    
    JsonObject state = doc.createNestedObject("state");
    state["all_on"] = group.on;
    state["any_on"] = group.on;
    
    JsonObject action = doc.createNestedObject("action");
    action["on"] = group.on;
    action["bri"] = group.bri;
    action["hue"] = group.hue;
    action["sat"] = group.sat;
    action["ct"] = group.ct;
    JsonArray xy = action.createNestedArray("xy");
    xy.add(group.xy[0]);
    xy.add(group.xy[1]);
    action["colormode"] = group.colormode == 1 ? "xy" : (group.colormode == 2 ? "ct" : "hs");
    
    String output;
    serializeJson(doc, output);
    return output;
}

String HueAPI::buildConfigJson(bool full) {
    DynamicJsonDocument doc(2048);
    
    doc["name"] = bridgeConfig.name;
    doc["bridgeid"] = bridgeConfig.bridgeid;
    doc["modelid"] = bridgeConfig.modelid;
    doc["swversion"] = bridgeConfig.swversion;
    doc["apiversion"] = bridgeConfig.apiversion;
    doc["mac"] = WiFi.macAddress();
    
    // Factory new status: true if no users exist
    doc["factorynew"] = (apiUsersCount == 0);
    doc["replacesbridgeid"] = nullptr;
    
    if (full) {
        doc["datastoreversion"] = "70";
        doc["starterkit"] = false;
        JsonObject whitelist = doc.createNestedObject("whitelist");
        for (uint8_t i = 0; i < apiUsersCount; i++) {
            JsonObject user = whitelist.createNestedObject(apiUsers[i].username);
            user["name"] = apiUsers[i].devicetype;
            user["create date"] = apiUsers[i].create_date;
            user["last use date"] = apiUsers[i].last_use_date;
        }
    }
    
    doc["linkbutton"] = bridgeConfig.linkbutton;
    doc["portalservices"] = false;
    doc["portalconnection"] = "disconnected";
    doc["portalstate"] = JsonObject();
    
    doc["zigbeechannel"] = 15;
    doc["dhcp"] = bridgeConfig.dhcp;
    doc["ipaddress"] = WiFi.localIP().toString();
    doc["netmask"] = WiFi.subnetMask().toString();
    doc["gateway"] = WiFi.gatewayIP().toString();
    doc["timezone"] = bridgeConfig.timezone;
    
    // Get actual time
    time_t now = time(nullptr);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    char utcStr[32];
    strftime(utcStr, sizeof(utcStr), "%Y-%m-%dT%H:%M:%S", &timeinfo);
    doc["UTC"] = utcStr;
    
    localtime_r(&now, &timeinfo);
    char localStr[32];
    strftime(localStr, sizeof(localStr), "%Y-%m-%dT%H:%M:%S", &timeinfo);
    doc["localtime"] = localStr;
    
    String output;
    serializeJson(doc, output);
    return output;
}

// Light control implementation (same as before)
bool setLightState(uint8_t lightId, JsonObject& state) {
    if (lightId >= lightsCount) return false;
    Light& light = lights[lightId];
    if (!light.reachable) return false;
    return sendLightCommand(lightId, state);
}

bool setGroupState(uint8_t groupId, JsonObject& state) {
    if (groupId >= groupsCount) return false;
    Group& group = groups[groupId];
    bool success = true;
    for (uint8_t i = 0; i < group.lights_count; i++) {
        if (!setLightState(group.lights[i], state)) {
            success = false;
        }
    }
    if (state.containsKey("on")) group.on = state["on"];
    if (state.containsKey("bri")) group.bri = state["bri"];
    if (state.containsKey("hue")) group.hue = state["hue"];
    if (state.containsKey("sat")) group.sat = state["sat"];
    if (state.containsKey("ct")) group.ct = state["ct"];
    if (state.containsKey("xy")) {
        group.xy[0] = state["xy"][0];
        group.xy[1] = state["xy"][1];
    }
    return success;
}

bool sendLightCommand(uint8_t lightId, JsonObject& state) {
    Light& light = lights[lightId];
    
    // Update local state cache
    if (state.containsKey("on")) light.on = state["on"];
    if (state.containsKey("bri")) light.bri = state["bri"];
    if (state.containsKey("hue")) {
        light.hue = state["hue"];
        light.colormode = 3;
    }
    if (state.containsKey("sat")) {
        light.sat = state["sat"];
        light.colormode = 3;
    }
    if (state.containsKey("ct")) {
        light.ct = state["ct"];
        light.colormode = 2;
    }
    if (state.containsKey("xy")) {
        light.xy[0] = state["xy"][0];
        light.xy[1] = state["xy"][1];
        light.colormode = 1;
    }
    
    // Build HTTP request based on protocol
    HTTPClient http;
    String url = "http://" + light.ip.toString() + "/state";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    
    String payload;
    bool isMultiSegment = (strcmp(light.protocol, "native_multi") == 0);
    
    if (isMultiSegment) {
        // native_multi: Use segment ID as key (1-indexed)
        DynamicJsonDocument doc(512);
        JsonObject segmentState = doc.createNestedObject(String(light.segment + 1));
        
        // Copy all state properties to segment object
        for (JsonPair kv : state) {
            segmentState[kv.key()] = kv.value();
        }
        
        serializeJson(doc, payload);
        Serial.printf("[LIGHT CONTROL] native_multi: Sending to %s (segment %d): %s\n", 
                      url.c_str(), light.segment + 1, payload.c_str());
    }
    else {
        // native_single: Send state directly
        serializeJson(state, payload);
        Serial.printf("[LIGHT CONTROL] native_single: Sending to %s: %s\n", 
                      url.c_str(), payload.c_str());
    }
    
    int httpCode = http.PUT(payload);
    http.end();
    
    if (httpCode == 200) {
        saveConfig();
        return true;
    } else {
        Serial.printf("[LIGHT CONTROL] Failed to control light %d, HTTP code: %d\n", lightId, httpCode);
        light.reachable = false;
        return false;
    }
}

String HueAPI::generateUsername() {
    String chars = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    String username = "";
    for (int i = 0; i < 40; i++) {
        username += chars[random(chars.length())];
    }
    return username;
}
