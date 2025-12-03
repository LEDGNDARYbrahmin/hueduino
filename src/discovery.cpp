#include "discovery.h"
#include "config.h"
#include "config_defaults.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

// SSDP Discovery Implementation
SSDPDiscovery::SSDPDiscovery() : multicastIP(239, 255, 255, 250), multicastPort(SSDP_MULTICAST_PORT), lastNotify(0) {}

void SSDPDiscovery::begin() {
    Serial.println("[SSDP] Starting SSDP discovery service...");
    
    if (udp.beginMulticast(multicastIP, multicastPort)) {
        Serial.printf("[SSDP] Listening on %s:%d\n", multicastIP.toString().c_str(), multicastPort);
    } else {
        Serial.println("[SSDP] Failed to start multicast");
    }
}

void SSDPDiscovery::handleClient() {
    int packetSize = udp.parsePacket();
    if (packetSize) {
        char packetBuffer[512];
        int len = udp.read(packetBuffer, sizeof(packetBuffer) - 1);
        if (len > 0) {
            packetBuffer[len] = 0;
            
            // Check if this is a M-SEARCH request
            if (strstr(packetBuffer, "M-SEARCH") && 
                (strstr(packetBuffer, "ssdp:all") || 
                 strstr(packetBuffer, "upnp:rootdevice") ||
                 strstr(packetBuffer, "IpBridge"))) {
                
                Serial.println("[SSDP] Received M-SEARCH request");
                sendResponse(udp.remoteIP(), udp.remotePort());
            }
        }
    }
    
    // Send periodic NOTIFY messages
    if (millis() - lastNotify > SSDP_NOTIFY_INTERVAL_MS) {
        sendNotify();
        lastNotify = millis();
    }
}

void SSDPDiscovery::sendResponse(IPAddress remoteIP, uint16_t remotePort) {
    String response = getSearchResponse();
    
    udp.beginPacket(remoteIP, remotePort);
    udp.write((uint8_t*)response.c_str(), response.length());
    udp.endPacket();
    
    Serial.printf("[SSDP] Sent response to %s:%d\n", remoteIP.toString().c_str(), remotePort);
}

String SSDPDiscovery::getSearchResponse() {
    IPAddress ip = WiFi.localIP();
    
    String response = "HTTP/1.1 200 OK\r\n";
    response += "HOST: " SSDP_MULTICAST_IP ":" + String(SSDP_MULTICAST_PORT) + "\r\n";
    response += "EXT:\r\n";
    response += "CACHE-CONTROL: max-age=" + String(SSDP_CACHE_MAX_AGE) + "\r\n";
    response += "LOCATION: http://" + ip.toString() + ":" + String(bridgeConfig.http_port) + "/description.xml\r\n";
    response += "SERVER: Linux/3.14.0 UPnP/1.0 IpBridge/1.56.0\r\n";
    response += "hue-bridgeid: " + String(bridgeConfig.bridgeid) + "\r\n";
    response += "ST: urn:schemas-upnp-org:device:basic:1\r\n";
    response += "USN: uuid:2f402f80-da50-11e1-9b23-" + String(bridgeConfig.bridgeid) + "\r\n\r\n";
    
    return response;
}

void SSDPDiscovery::sendNotify() {
    String notify = "NOTIFY * HTTP/1.1\r\n";
    notify += "HOST: " SSDP_MULTICAST_IP ":" + String(SSDP_MULTICAST_PORT) + "\r\n";
    notify += "CACHE-CONTROL: max-age=" + String(SSDP_CACHE_MAX_AGE) + "\r\n";
    notify += "LOCATION: http://" + WiFi.localIP().toString() + ":" + String(bridgeConfig.http_port) + "/description.xml\r\n";
    notify += "SERVER: Linux/3.14.0 UPnP/1.0 IpBridge/1.56.0\r\n";
    notify += "NTS: ssdp:alive\r\n";
    notify += "NT: urn:schemas-upnp-org:device:basic:1\r\n";
    notify += "USN: uuid:2f402f80-da50-11e1-9b23-" + String(bridgeConfig.bridgeid) + "\r\n\r\n";
    
    udp.beginPacket(multicastIP, multicastPort);
    udp.write((uint8_t*)notify.c_str(), notify.length());
    udp.endPacket();
}

// mDNS Service Implementation
MDNSService::MDNSService() {}

void MDNSService::begin(const char* hostname) {
    strcpy(this->hostname, hostname);
    
    Serial.printf("[mDNS] Starting mDNS service with hostname: %s\n", hostname);
    
    if (!MDNS.begin(hostname)) {
        Serial.println("[mDNS] Error starting mDNS");
        return;
    }
    
    // Add Hue bridge service
    // Add Hue bridge service
    MDNS.addService("hue", "tcp", bridgeConfig.http_port);
    MDNS.addServiceTxt("hue", "tcp", "bridgeid", String(bridgeConfig.bridgeid));
    MDNS.addServiceTxt("hue", "tcp", "modelid", String(bridgeConfig.modelid));
    
    // Also advertise standard HTTP
    MDNS.addService("http", "tcp", bridgeConfig.http_port);
    MDNS.addServiceTxt("http", "tcp", "bridgeid", String(bridgeConfig.bridgeid));
    
    Serial.println("[mDNS] mDNS service started successfully");
}

void MDNSService::update() {
    // mDNS is handled automatically by ESP32 framework
}

// Light Discovery Implementation
LightDiscovery::LightDiscovery() {}

void LightDiscovery::scanNetwork() {
    Serial.println("[DISCOVERY] Scanning network for lights...");
    
    IPAddress localIP = WiFi.localIP();
    IPAddress subnet = WiFi.subnetMask();
    
    // Calculate network range (BIG ENDIAN - correct byte order)
    uint32_t ip = (localIP[0] << 24) | (localIP[1] << 16) | (localIP[2] << 8) | localIP[3];
    uint32_t mask = (subnet[0] << 24) | (subnet[1] << 16) | (subnet[2] << 8) | subnet[3];
    uint32_t network = ip & mask;
    uint32_t broadcast = network | ~mask;
    
    // Limit scan to a smaller range around the bridge IP for speed
    // Configurable via LIGHT_SCAN_RANGE in config_defaults.h
    
    uint32_t startIP = ip > LIGHT_SCAN_RANGE ? ip - LIGHT_SCAN_RANGE : network + 1; 
    uint32_t endIP = ip + LIGHT_SCAN_RANGE;
    if (endIP >= broadcast) endIP = broadcast - 1;

    Serial.printf("[DISCOVERY] Scanning range: %d.%d.%d.%d to %d.%d.%d.%d\n", 
        (startIP >> 24) & 0xFF, (startIP >> 16) & 0xFF, (startIP >> 8) & 0xFF, startIP & 0xFF,
        (endIP >> 24) & 0xFF, (endIP >> 16) & 0xFF, (endIP >> 8) & 0xFF, endIP & 0xFF);

    for (uint32_t i = startIP; i <= endIP; i++) {
        IPAddress testIP((i >> 24) & 0xFF, (i >> 16) & 0xFF, (i >> 8) & 0xFF, i & 0xFF);
        
        if (testIP == localIP) continue; // Skip self
        
        // Use a very short timeout for detection
        if (detectLight(testIP)) {
            Serial.printf("[DISCOVERY] Found light at %s\n", testIP.toString().c_str());
        }
        
        // Yield to prevent watchdog triggers
        delay(1); 
    }
    
    Serial.printf("[DISCOVERY] Scan complete. Found %d lights\n", lightsCount);
}

bool LightDiscovery::detectLight(IPAddress ip) {
    HTTPClient http;
    
    // 1. Try diyHue /detect endpoint first (fastest)
    String url = "http://" + ip.toString() + LIGHT_DETECT_ENDPOINT;
    http.begin(url);
    http.setConnectTimeout(LIGHT_DETECT_TIMEOUT_MS); // Fast fail on connection
    http.setTimeout(LIGHT_DETECT_TIMEOUT_MS);
    int httpCode = http.GET();
    
    if (httpCode == 200) {
        String payload = http.getString();
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (!error && doc.containsKey("protocol")) {
            addLight(ip, doc);
            http.end();
            return true;
        }
    }
    http.end();
    
    // 2. Try generic /description.xml (UPnP/Hue emulation)
    // This allows discovering other diyHue nodes or WLED instances
    url = "http://" + ip.toString() + "/description.xml";
    http.begin(url);
    http.setConnectTimeout(LIGHT_DETECT_TIMEOUT_MS); // Fast fail on connection
    http.setTimeout(LIGHT_DETECT_TIMEOUT_MS);
    httpCode = http.GET();
    
    if (httpCode == 200) {
        String payload = http.getString();
        // Simple check for "Philips hue" or "WLED" in description
        if (payload.indexOf("Philips hue") >= 0 || payload.indexOf("WLED") >= 0) {
            // Create a basic light config for this discovered device
            StaticJsonDocument<512> doc;
            doc["name"] = "Discovered Light";
            doc["modelid"] = "LCT015";
            doc["type"] = "Extended color light";
            doc["protocol"] = "native_single";
            
            addLight(ip, doc);
            http.end();
            return true;
        }
    }
    http.end();

    return false;
}

void LightDiscovery::addLight(IPAddress ip, const JsonDocument& lightInfo) {
    // Parse light information
    const char* name = lightInfo["name"] | "Hue Light";
    const char* modelid = lightInfo["modelid"] | DEFAULT_LIGHT_MODEL;
    const char* type = lightInfo["type"] | DEFAULT_LIGHT_TYPE;
    const char* protocol = lightInfo["protocol"] | "native_single";
    uint16_t lightCount = lightInfo["lights"] | 1;
    
    // Check if this is a multi-segment device
    bool isMultiSegment = (strcmp(protocol, "native_multi") == 0 && lightCount > 1);
    
    if (isMultiSegment) {
        // Create virtual lights for each segment
        Serial.printf("[DISCOVERY] Creating %d virtual lights for multi-segment device at %s\n", 
                      lightCount, ip.toString().c_str());
        
        for (uint16_t segment = 0; segment < lightCount; segment++) {
            if (lightsCount >= MAX_LIGHTS) {
                Serial.println("[DISCOVERY] Maximum lights reached");
                return;
            }
            
            // Check if this segment already exists
            bool exists = false;
            for (uint8_t i = 0; i < lightsCount; i++) {
                if (lights[i].ip == ip && lights[i].segment == segment) {
                    Serial.printf("[DISCOVERY] Segment %d at %s already exists\n", segment, ip.toString().c_str());
                    exists = true;
                    break;
                }
            }
            
            if (exists) continue;
            
            // Add new virtual light for this segment
            Light& light = lights[lightsCount];
            
            // Create segment-specific name
            char segmentName[32];
            snprintf(segmentName, sizeof(segmentName), "%s - Seg %d", name, segment + 1);
            strncpy(light.name, segmentName, sizeof(light.name) - 1);
            
            strncpy(light.modelid, modelid, sizeof(light.modelid) - 1);
            strncpy(light.type, type, sizeof(light.type) - 1);
            strncpy(light.protocol, protocol, sizeof(light.protocol) - 1);
            strcpy(light.manufacturername, DEFAULT_MANUFACTURER);
            strcpy(light.swversion, DEFAULT_SW_VERSION);
            
            light.ip = ip;
            light.segment = segment;
            light.lights_count = lightCount;
            
            // Generate unique ID for this segment
            String uniqueid = generateUniqueId();
            strncpy(light.uniqueid, uniqueid.c_str(), sizeof(light.uniqueid) - 1);
            
            // Default state
            light.on = false;
            light.bri = LIGHT_DEFAULT_BRI;
            light.hue = LIGHT_DEFAULT_HUE;
            light.sat = LIGHT_DEFAULT_SAT;
            light.ct = LIGHT_DEFAULT_CT;
            light.xy[0] = 0.0;
            light.xy[1] = 0.0;
            light.colormode = 2;
            light.reachable = true;
            
            // Capabilities
            light.supports_color = true;
            light.supports_ct = true;
            light.ct_min = LIGHT_CT_MIN;
            light.ct_max = LIGHT_CT_MAX;
            
            Serial.printf("[DISCOVERY] Added virtual light %d: %s (segment %d/%d)\n", 
                          lightsCount, light.name, segment + 1, lightCount);
            
            lightsCount++;
        }
    }
    else {
        // Single light device (native_single or gradient)
        if (lightsCount >= MAX_LIGHTS) {
            Serial.println("[DISCOVERY] Maximum lights reached");
            return;
        }
        
        // Check if light already exists
        for (uint8_t i = 0; i < lightsCount; i++) {
            if (lights[i].ip == ip) {
                Serial.printf("[DISCOVERY] Light at %s already exists\n", ip.toString().c_str());
                return;
            }
        }
        
        // Add new light
        Light& light = lights[lightsCount];
        
        strncpy(light.name, name, sizeof(light.name) - 1);
        strncpy(light.modelid, modelid, sizeof(light.modelid) - 1);
        strncpy(light.type, type, sizeof(light.type) - 1);
        strncpy(light.protocol, protocol, sizeof(light.protocol) - 1);
        strcpy(light.manufacturername, DEFAULT_MANUFACTURER);
        strcpy(light.swversion, DEFAULT_SW_VERSION);
        
        light.ip = ip;
        light.segment = 0;  // Single light, no segment
        light.lights_count = 1;
        
        // Generate unique ID
        String uniqueid = generateUniqueId();
        strncpy(light.uniqueid, uniqueid.c_str(), sizeof(light.uniqueid) - 1);
        
        // Default state
        light.on = false;
        light.bri = LIGHT_DEFAULT_BRI;
        light.hue = LIGHT_DEFAULT_HUE;
        light.sat = LIGHT_DEFAULT_SAT;
        light.ct = LIGHT_DEFAULT_CT;
        light.xy[0] = 0.0;
        light.xy[1] = 0.0;
        light.colormode = 2;
        light.reachable = true;
        
        // Capabilities
        light.supports_color = true;
        light.supports_ct = true;
        light.ct_min = LIGHT_CT_MIN;
        light.ct_max = LIGHT_CT_MAX;
        
        Serial.printf("[DISCOVERY] Added light %d: %s at %s\n", 
                      lightsCount, light.name, ip.toString().c_str());
        
        lightsCount++;
    }
    
    // Save to NVS
    saveConfig();
}
