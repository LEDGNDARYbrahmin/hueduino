#ifndef DISCOVERY_H
#define DISCOVERY_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>

// SSDP Discovery for Hue Bridge
class SSDPDiscovery {
public:
    SSDPDiscovery();
    void begin();
    void handleClient();
    void sendNotify();
    
private:
    WiFiUDP udp;
    IPAddress multicastIP;
    uint16_t multicastPort;
    unsigned long lastNotify;
    
    void sendResponse(IPAddress remoteIP, uint16_t remotePort);
    String getSearchResponse();
};

// mDNS Service
class MDNSService {
public:
    MDNSService();
    void begin(const char* hostname);
    void update();
    
private:
    char hostname[32];
};

// Light Discovery (scan network for ESP8266 lights)
class LightDiscovery {
public:
    LightDiscovery();
    void scanNetwork();
    bool detectLight(IPAddress ip);
    void addLight(IPAddress ip, const JsonDocument& lightInfo);
    
private:
    void parseDetectResponse(const JsonDocument& doc, IPAddress ip);
};

#endif // DISCOVERY_H
