#include "entertainment.h"
#include "storage_manager.h"
#include "config.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Global entertainment manager
EntertainmentManager entertainmentManager;

// External storage manager
extern StorageManager storageManager;

EntertainmentManager::EntertainmentManager() : _areasCount(0) {
    memset(_areas, 0, sizeof(_areas));
    memset(_packetBuffer, 0, sizeof(_packetBuffer));
}

bool EntertainmentManager::begin() {
    Serial.println("[ENTERTAINMENT] Initializing Entertainment Manager...");
    
    // Start UDP server on port 2100
    if (_udp.begin(ENTERTAINMENT_UDP_PORT)) {
        Serial.printf("[ENTERTAINMENT] UDP server started on port %d\n", ENTERTAINMENT_UDP_PORT);
    } else {
        Serial.println("[ENTERTAINMENT] Failed to start UDP server");
        return false;
    }
    
    // Load areas from NVS
    if (loadAreas()) {
        Serial.printf("[ENTERTAINMENT] Loaded %d entertainment areas\n", _areasCount);
    } else {
        Serial.println("[ENTERTAINMENT] No saved areas found");
    }
    
    return true;
}

bool EntertainmentManager::createArea(const char* name, EntertainmentClass classType) {
    if (_areasCount >= MAX_ENTERTAINMENT_AREAS) {
        Serial.println("[ENTERTAINMENT] Maximum areas reached");
        return false;
    }
    
    EntertainmentArea& area = _areas[_areasCount];
    
    // Generate UUID
    String uuid = generateAreaId();
    strcpy(area.id, uuid.c_str());
    
    // Set name
    strncpy(area.name, name, sizeof(area.name) - 1);
    
    // Set properties
    area.active = false;
    area.class_type = classType;
    area.proxy_mode = PROXY_MODE_AUTO;
    area.proxy_node = 0;
    area.channels_count = 0;
    area.streaming = false;
    area.last_packet_time = 0;
    area.packets_received = 0;
    area.packets_dropped = 0;
    
    _areasCount++;
    
    Serial.printf("[ENTERTAINMENT] Created area: %s (ID: %s)\n", name, area.id);
    
    // Save to NVS
    saveAreas();
    
    return true;
}

bool EntertainmentManager::deleteArea(const char* areaId) {
    int index = findAreaIndex(areaId);
    if (index < 0) {
        Serial.printf("[ENTERTAINMENT] Area not found: %s\n", areaId);
        return false;
    }
    
    // Stop streaming if active
    if (_areas[index].streaming) {
        stopStreaming(areaId);
    }
    
    Serial.printf("[ENTERTAINMENT] Deleting area: %s\n", _areas[index].name);
    
    // Shift remaining areas
    for (int i = index; i < _areasCount - 1; i++) {
        _areas[i] = _areas[i + 1];
    }
    
    _areasCount--;
    
    // Save to NVS
    saveAreas();
    
    return true;
}

EntertainmentArea* EntertainmentManager::getArea(const char* areaId) {
    int index = findAreaIndex(areaId);
    if (index < 0) {
        return nullptr;
    }
    return &_areas[index];
}

EntertainmentArea* EntertainmentManager::getAreaByIndex(uint8_t index) {
    if (index >= _areasCount) {
        return nullptr;
    }
    return &_areas[index];
}

bool EntertainmentManager::addChannel(const char* areaId, uint8_t lightIndex,
                                     uint8_t posX, uint8_t posY, uint8_t posZ) {
    int index = findAreaIndex(areaId);
    if (index < 0) {
        return false;
    }
    
    EntertainmentArea& area = _areas[index];
    
    if (area.channels_count >= MAX_CHANNELS_PER_AREA) {
        Serial.println("[ENTERTAINMENT] Maximum channels reached for area");
        return false;
    }
    
    EntertainmentChannel& channel = area.channels[area.channels_count];
    channel.channel_id = area.channels_count;
    channel.light_index = lightIndex;
    channel.position_x = posX;
    channel.position_y = posY;
    channel.position_z = posZ;
    
    area.channels_count++;
    
    Serial.printf("[ENTERTAINMENT] Added channel %d to area %s (light %d)\n",
                  channel.channel_id, area.name, lightIndex);
    
    // Save to NVS
    saveAreas();
    
    return true;
}

bool EntertainmentManager::removeChannel(const char* areaId, uint8_t channelId) {
    int index = findAreaIndex(areaId);
    if (index < 0) {
        return false;
    }
    
    EntertainmentArea& area = _areas[index];
    
    if (channelId >= area.channels_count) {
        return false;
    }
    
    // Shift remaining channels
    for (uint8_t i = channelId; i < area.channels_count - 1; i++) {
        area.channels[i] = area.channels[i + 1];
        area.channels[i].channel_id = i; // Update channel IDs
    }
    
    area.channels_count--;
    
    Serial.printf("[ENTERTAINMENT] Removed channel %d from area %s\n", channelId, area.name);
    
    // Save to NVS
    saveAreas();
    
    return true;
}

bool EntertainmentManager::startStreaming(const char* areaId) {
    int index = findAreaIndex(areaId);
    if (index < 0) {
        return false;
    }
    
    EntertainmentArea& area = _areas[index];
    area.streaming = true;
    area.active = true;
    area.last_packet_time = millis();
    area.packets_received = 0;
    area.packets_dropped = 0;
    
    Serial.printf("[ENTERTAINMENT] Started streaming for area: %s\n", area.name);
    
    return true;
}

bool EntertainmentManager::stopStreaming(const char* areaId) {
    int index = findAreaIndex(areaId);
    if (index < 0) {
        return false;
    }
    
    EntertainmentArea& area = _areas[index];
    area.streaming = false;
    area.active = false;
    
    Serial.printf("[ENTERTAINMENT] Stopped streaming for area: %s\n", area.name);
    Serial.printf("[ENTERTAINMENT] Stats - Received: %u, Dropped: %u\n",
                  area.packets_received, area.packets_dropped);
    
    return true;
}

bool EntertainmentManager::isStreaming(const char* areaId) {
    int index = findAreaIndex(areaId);
    if (index < 0) {
        return false;
    }
    return _areas[index].streaming;
}

void EntertainmentManager::handleUDP() {
    int packetSize = _udp.parsePacket();
    if (packetSize == 0) {
        return;
    }
    
    // Read packet
    int len = _udp.read(_packetBuffer, sizeof(_packetBuffer));
    if (len <= 0) {
        return;
    }
    
    // Find active streaming area
    for (uint8_t i = 0; i < _areasCount; i++) {
        if (_areas[i].streaming) {
            if (parsePacket(_packetBuffer, len, _areas[i])) {
                _areas[i].packets_received++;
                _areas[i].last_packet_time = millis();
            } else {
                _areas[i].packets_dropped++;
            }
            break; // Only one area can stream at a time
        }
    }
}

void EntertainmentManager::printStats() {
    Serial.println("[ENTERTAINMENT] ========================================");
    Serial.printf("[ENTERTAINMENT] Total Areas: %d\n", _areasCount);
    Serial.println("[ENTERTAINMENT] ========================================");
    
    for (uint8_t i = 0; i < _areasCount; i++) {
        EntertainmentArea& area = _areas[i];
        Serial.printf("[ENTERTAINMENT] %d. %s\n", i + 1, area.name);
        Serial.printf("[ENTERTAINMENT]    ID: %s\n", area.id);
        Serial.printf("[ENTERTAINMENT]    Active: %s\n", area.active ? "Yes" : "No");
        Serial.printf("[ENTERTAINMENT]    Streaming: %s\n", area.streaming ? "Yes" : "No");
        Serial.printf("[ENTERTAINMENT]    Channels: %d\n", area.channels_count);
        Serial.printf("[ENTERTAINMENT]    Packets RX: %u\n", area.packets_received);
        Serial.printf("[ENTERTAINMENT]    Packets Dropped: %u\n", area.packets_dropped);
    }
    
    Serial.println("[ENTERTAINMENT] ========================================");
}

void EntertainmentManager::resetStats(const char* areaId) {
    int index = findAreaIndex(areaId);
    if (index < 0) {
        return;
    }
    
    _areas[index].packets_received = 0;
    _areas[index].packets_dropped = 0;
}

bool EntertainmentManager::saveAreas() {
    Serial.printf("[ENTERTAINMENT] Saving %d areas to NVS...\n", _areasCount);
    
    bool success = storageManager.saveArray("entertainment", "area", 
                                           _areas, _areasCount, MAX_ENTERTAINMENT_AREAS);
    
    if (success) {
        Serial.println("[ENTERTAINMENT] Areas saved successfully");
    } else {
        Serial.println("[ENTERTAINMENT] Failed to save areas");
    }
    
    return success;
}

bool EntertainmentManager::loadAreas() {
    Serial.println("[ENTERTAINMENT] Loading areas from NVS...");
    
    bool success = storageManager.loadArray("entertainment", "area", 
                                           _areas, _areasCount, MAX_ENTERTAINMENT_AREAS);
    
    if (success) {
        Serial.printf("[ENTERTAINMENT] Loaded %d areas successfully\n", _areasCount);
    } else {
        Serial.println("[ENTERTAINMENT] Failed to load areas");
    }
    
    return success;
}

// Private helper methods

int EntertainmentManager::findAreaIndex(const char* areaId) {
    for (int i = 0; i < _areasCount; i++) {
        if (strcmp(_areas[i].id, areaId) == 0) {
            return i;
        }
    }
    return -1;
}

bool EntertainmentManager::parsePacket(const uint8_t* data, size_t len, EntertainmentArea& area) {
    // Minimum packet size: header (16 bytes) + at least one RGB color (6 bytes)
    if (len < 22) {
        return false;
    }
    
    // Parse header
    EntertainmentPacketHeader* header = (EntertainmentPacketHeader*)data;
    
    // Verify protocol
    if (strncmp(header->protocol, "HueStream", 9) != 0) {
        return false;
    }
    
    // Parse color data
    const uint8_t* colorData = data + sizeof(EntertainmentPacketHeader);
    size_t colorDataLen = len - sizeof(EntertainmentPacketHeader);
    
    // Each color is 7 bytes: channel_id (1) + R (2) + G (2) + B (2)
    uint8_t colorCount = colorDataLen / 7;
    
    if (colorCount == 0 || colorCount > area.channels_count) {
        return false;
    }
    
    // Extract RGB colors
    RGBColor colors[MAX_CHANNELS_PER_AREA];
    for (uint8_t i = 0; i < colorCount; i++) {
        const uint8_t* colorPtr = colorData + (i * 7);
        uint8_t channelId = colorPtr[0];
        
        if (channelId >= area.channels_count) {
            continue;
        }
        
        colors[channelId].r = (colorPtr[1] << 8) | colorPtr[2];
        colors[channelId].g = (colorPtr[3] << 8) | colorPtr[4];
        colors[channelId].b = (colorPtr[5] << 8) | colorPtr[6];
    }
    
    // Apply colors to lights
    return applyColorData(area, colors, colorCount);
}

bool EntertainmentManager::applyColorData(EntertainmentArea& area, const RGBColor* colors, uint8_t count) {
    bool success = true;
    
    for (uint8_t i = 0; i < count && i < area.channels_count; i++) {
        EntertainmentChannel& channel = area.channels[i];
        const RGBColor& color = colors[i];
        
        if (channel.light_index >= lightsCount) {
            continue;
        }
        
        Light& light = lights[channel.light_index];
        
        // Convert RGB to Hue/Sat/Bri
        uint16_t hue;
        uint8_t sat, bri;
        rgbToHueSat(color.r, color.g, color.b, hue, sat, bri);
        
        // Build JSON payload
        StaticJsonDocument<256> doc;
        doc["on"] = true;
        doc["hue"] = hue;
        doc["sat"] = sat;
        doc["bri"] = bri;
        doc["transitiontime"] = 0; // Instant for entertainment mode
        
        String payload;
        serializeJson(doc, payload);
        
        // Send to light (non-blocking, fire and forget)
        HTTPClient http;
        String url = "http://" + light.ip.toString() + "/state";
        
        http.begin(url);
        http.setTimeout(50); // Very short timeout for low latency
        http.addHeader("Content-Type", "application/json");
        
        int httpCode = http.PUT(payload);
        http.end();
        
        if (httpCode != 200) {
            success = false;
        } else {
            // Update local state
            light.on = true;
            light.hue = hue;
            light.sat = sat;
            light.bri = bri;
            light.colormode = 3; // HS mode
        }
    }
    
    return success;
}

void EntertainmentManager::rgbToHueSat(uint16_t r, uint16_t g, uint16_t b,
                                       uint16_t& hue, uint8_t& sat, uint8_t& bri) {
    // Normalize to 0-1 range
    float rf = r / 65535.0;
    float gf = g / 65535.0;
    float bf = b / 65535.0;
    
    // Find max and min
    float maxVal = std::max(rf, std::max(gf, bf));
    float minVal = std::min(rf, std::min(gf, bf));
    float delta = maxVal - minVal;
    
    // Calculate brightness (0-254)
    bri = (uint8_t)(maxVal * 254.0);
    
    // Calculate saturation (0-254)
    if (maxVal == 0) {
        sat = 0;
    } else {
        sat = (uint8_t)((delta / maxVal) * 254.0);
    }
    
    // Calculate hue (0-65535)
    if (delta == 0) {
        hue = 0;
    } else {
        float h;
        if (maxVal == rf) {
            h = (gf - bf) / delta + (gf < bf ? 6 : 0);
        } else if (maxVal == gf) {
            h = (bf - rf) / delta + 2;
        } else {
            h = (rf - gf) / delta + 4;
        }
        h /= 6.0;
        hue = (uint16_t)(h * 65535.0);
    }
}

String EntertainmentManager::generateAreaId() {
    // Generate UUID-like ID
    char uuid[37];
    snprintf(uuid, sizeof(uuid), "%08x-%04x-%04x-%04x-%012x",
             random(0xFFFFFFFF), random(0xFFFF), random(0xFFFF),
             random(0xFFFF), random(0xFFFFFFFF));
    return String(uuid);
}
