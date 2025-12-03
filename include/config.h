#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <ArduinoJson.h>

// Version information
#define BRIDGE_VERSION "1.0.0"
#define BRIDGE_API_VERSION "1.46.0"
#define BRIDGE_SW_VERSION "1948086000"

// Network configuration
#define DEFAULT_HTTP_PORT 80
#define DEFAULT_HTTPS_PORT 443
#define ENTERTAINMENT_UDP_PORT 2100

// Bridge identification
#define BRIDGE_MODEL_ID "BSB002"
#define BRIDGE_MANUFACTURER "Signify"

// Configuration limits
#define MAX_LIGHTS 50
#define MAX_GROUPS 16
#define MAX_SCENES 100
#define MAX_SCHEDULES 100
#define MAX_SENSORS 60
#define MAX_RULES 200
#define MAX_API_USERS 50

// NVS namespaces
#define NVS_NAMESPACE "hue_bridge"
#define NVS_CONFIG "config"
#define NVS_LIGHTS "lights"
#define NVS_GROUPS "groups"
#define NVS_SCENES "scenes"

// Configuration structure
struct BridgeConfig {
    char name[32];
    char bridgeid[17];  // MAC-based unique ID
    char apiversion[16];
    char swversion[16];
    char modelid[16];
    char manufacturer[32];
    bool linkbutton;
    uint32_t linkbutton_timestamp;
    char timezone[32];
    bool dhcp;
    IPAddress ipaddress;
    IPAddress netmask;
    IPAddress gateway;
    uint16_t http_port;
    uint16_t https_port;
    bool discovery_enabled;
};

// Light structure (matches ESP8266 light protocol)
struct Light {
    char uniqueid[32];
    char name[32];
    char type[32];
    char modelid[16];
    char manufacturername[32];
    char swversion[16];
    IPAddress ip;
    char protocol[16];      // "native_single", "native_multi", "gradient"
    uint8_t segment;        // Segment index (0-based), 0 for single lights
    uint16_t lights_count;  // For multi-light strips
    uint16_t pixel_count;   // Total pixels in the strip
    uint16_t divided_lights[10]; // Pixels per section (max 10 sections)
    uint8_t transition_leds; // Transition pixels between sections
    
    // State
    bool on;
    uint8_t bri;
    uint16_t hue;
    uint8_t sat;
    uint16_t ct;
    float xy[2];
    uint8_t colormode;  // 1=xy, 2=ct, 3=hs
    bool reachable;
    
    // Capabilities
    bool supports_color;
    bool supports_ct;
    uint16_t ct_min;
    uint16_t ct_max;
};

// Group structure
struct Group {
    char name[32];
    char type[16];
    uint8_t lights[MAX_LIGHTS];
    uint8_t lights_count;
    bool on;
    uint8_t bri;
    uint16_t hue;
    uint8_t sat;
    uint16_t ct;
    float xy[2];
    uint8_t colormode;
};

// Scene structure
struct Scene {
    char name[32];
    char type[16];
    uint8_t group;
    uint8_t lights[MAX_LIGHTS];
    uint8_t lights_count;
    bool recycle;
    bool locked;
    char appdata[64];
    uint32_t version;
};

// User/API key structure
struct ApiUser {
    char username[64];
    char devicetype[64];
    char create_date[32];
    char last_use_date[32];
};

// Global configuration instance
extern BridgeConfig bridgeConfig;
extern Light lights[MAX_LIGHTS];
extern Group groups[MAX_GROUPS];
extern Scene scenes[MAX_SCENES];
extern ApiUser apiUsers[MAX_API_USERS];
extern uint8_t lightsCount;
extern uint8_t groupsCount;
extern uint8_t scenesCount;
extern uint8_t apiUsersCount;

// Configuration functions
void initConfig();
bool loadConfig();
bool saveConfig();
bool loadLights();
bool saveLights();
String generateBridgeId();
String generateUniqueId();
String generateApiKey();

#endif // CONFIG_H
