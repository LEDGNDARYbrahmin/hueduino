#ifndef ENTERTAINMENT_H
#define ENTERTAINMENT_H

#include <Arduino.h>
#include <WiFiUdp.h>
#include "config.h"

// Entertainment configuration
#define ENTERTAINMENT_UDP_PORT 2100
#define MAX_ENTERTAINMENT_AREAS 10
#define MAX_CHANNELS_PER_AREA 20

/**
 * Entertainment Mode (Hue Sync)
 * 
 * Enables high-speed, low-latency streaming of color data to lights.
 * Used for:
 * - Hue Sync (screen mirroring)
 * - Gaming integrations
 * - Music visualization
 * - Ambilight effects
 * 
 * Protocol: UDP on port 2100
 * Update rate: Up to 60 Hz
 * Latency: <20ms
 */

// Entertainment stream proxy modes
enum StreamProxyMode {
    PROXY_MODE_AUTO,
    PROXY_MODE_MANUAL
};

// Entertainment class types
enum EntertainmentClass {
    CLASS_TV,
    CLASS_MUSIC,
    CLASS_GAME,
    CLASS_OTHER
};

// Channel configuration
struct EntertainmentChannel {
    uint8_t channel_id;
    uint8_t light_index;         // Index in lights array
    uint8_t position_x;          // 0-255 (left to right)
    uint8_t position_y;          // 0-255 (bottom to top)
    uint8_t position_z;          // 0-255 (back to front)
};

// Entertainment area/group
struct EntertainmentArea {
    char id[37];                 // UUID
    char name[64];
    bool active;
    EntertainmentClass class_type;
    StreamProxyMode proxy_mode;
    uint8_t proxy_node;
    
    // Channels (light mappings)
    EntertainmentChannel channels[MAX_CHANNELS_PER_AREA];
    uint8_t channels_count;
    
    // Stream state
    bool streaming;
    unsigned long last_packet_time;
    uint32_t packets_received;
    uint32_t packets_dropped;
};

// Entertainment packet header (DTLS optional)
struct EntertainmentPacketHeader {
    char protocol[9];            // "HueStream"
    uint8_t version_major;
    uint8_t version_minor;
    uint8_t sequence;
    uint16_t reserved;
    uint8_t color_space;         // 0=RGB, 1=XY
    uint8_t reserved2;
};

// RGB color data
struct RGBColor {
    uint16_t r;                  // 0-65535
    uint16_t g;
    uint16_t b;
};

/**
 * Entertainment Manager
 * 
 * Manages entertainment areas and handles UDP streaming
 */
class EntertainmentManager {
public:
    EntertainmentManager();
    
    // Initialize entertainment system
    bool begin();
    
    // Area management
    bool createArea(const char* name, EntertainmentClass classType);
    bool deleteArea(const char* areaId);
    EntertainmentArea* getArea(const char* areaId);
    EntertainmentArea* getAreaByIndex(uint8_t index);
    uint8_t getAreasCount() const { return _areasCount; }
    
    // Channel management
    bool addChannel(const char* areaId, uint8_t lightIndex, 
                   uint8_t posX, uint8_t posY, uint8_t posZ);
    bool removeChannel(const char* areaId, uint8_t channelId);
    
    // Streaming control
    bool startStreaming(const char* areaId);
    bool stopStreaming(const char* areaId);
    bool isStreaming(const char* areaId);
    
    // UDP handling (call in loop())
    void handleUDP();
    
    // Statistics
    void printStats();
    void resetStats(const char* areaId);
    
    // Persistence
    bool saveAreas();
    bool loadAreas();
    
private:
    WiFiUDP _udp;
    EntertainmentArea _areas[MAX_ENTERTAINMENT_AREAS];
    uint8_t _areasCount;
    
    // Packet buffer
    uint8_t _packetBuffer[1024];
    
    // Helper methods
    int findAreaIndex(const char* areaId);
    bool parsePacket(const uint8_t* data, size_t len, EntertainmentArea& area);
    bool applyColorData(EntertainmentArea& area, const RGBColor* colors, uint8_t count);
    void rgbToHueSat(uint16_t r, uint16_t g, uint16_t b, uint16_t& hue, uint8_t& sat, uint8_t& bri);
    String generateAreaId();
};

// Global entertainment manager
extern EntertainmentManager entertainmentManager;

#endif // ENTERTAINMENT_H
