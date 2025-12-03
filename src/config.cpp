#include "config.h"
#include <Preferences.h>
#include <WiFi.h>

// Global instances
BridgeConfig bridgeConfig;
Light lights[MAX_LIGHTS];
Group groups[MAX_GROUPS];
Scene scenes[MAX_SCENES];
ApiUser apiUsers[MAX_API_USERS];
uint8_t lightsCount = 0;
uint8_t groupsCount = 0;
uint8_t scenesCount = 0;
uint8_t apiUsersCount = 0;

Preferences preferences;

// Generate unique Bridge ID from MAC address
String generateBridgeId() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char bridgeid[18];
    sprintf(bridgeid, "%02X%02X%02X%02X%02X%02X", 
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(bridgeid);
}

// Generate unique ID for lights (MAC-based)
String generateUniqueId() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char uniqueid[32];
    sprintf(uniqueid, "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
            mac[0], mac[1], mac[2], 0xff, 0xfe, mac[3], mac[4], mac[5]);
    return String(uniqueid);
}

// Generate random API key
String generateApiKey() {
    const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    char apikey[41];
    
    for (int i = 0; i < 40; i++) {
        apikey[i] = charset[random(0, sizeof(charset) - 1)];
    }
    apikey[40] = '\0';
    
    return String(apikey);
}

// Initialize default configuration
void initConfig() {
    Serial.println("[CONFIG] Initializing default configuration...");
    
    // Bridge configuration
    strcpy(bridgeConfig.name, "diyHue Bridge");
    String bridgeid = generateBridgeId();
    strcpy(bridgeConfig.bridgeid, bridgeid.c_str());
    strcpy(bridgeConfig.apiversion, BRIDGE_API_VERSION);
    strcpy(bridgeConfig.swversion, BRIDGE_SW_VERSION);
    strcpy(bridgeConfig.modelid, BRIDGE_MODEL_ID);
    strcpy(bridgeConfig.manufacturer, BRIDGE_MANUFACTURER);
    bridgeConfig.linkbutton = false;
    bridgeConfig.linkbutton_timestamp = 0;
    strcpy(bridgeConfig.timezone, "UTC");
    bridgeConfig.dhcp = true;
    bridgeConfig.http_port = DEFAULT_HTTP_PORT;
    bridgeConfig.https_port = DEFAULT_HTTPS_PORT;
    bridgeConfig.discovery_enabled = true;
    
    // Initialize default group 0 (all lights)
    strcpy(groups[0].name, "All lights");
    strcpy(groups[0].type, "LightGroup");
    groups[0].lights_count = 0;
    groups[0].on = false;
    groups[0].bri = 254;
    groups[0].hue = 0;
    groups[0].sat = 0;
    groups[0].ct = 200;
    groups[0].xy[0] = 0.0;
    groups[0].xy[1] = 0.0;
    groups[0].colormode = 2;
    groupsCount = 1;
    
    Serial.println("[CONFIG] Default configuration initialized");
    Serial.printf("[CONFIG] Bridge ID: %s\n", bridgeConfig.bridgeid);
}

// Load configuration from NVS
bool loadConfig() {
    Serial.println("[CONFIG] Loading configuration from NVS...");
    
    if (!preferences.begin(NVS_NAMESPACE, false)) {
        Serial.println("[CONFIG] Failed to open NVS namespace");
        return false;
    }
    
    // Load bridge config
    size_t len = preferences.getBytes(NVS_CONFIG, &bridgeConfig, sizeof(BridgeConfig));
    if (len != sizeof(BridgeConfig)) {
        Serial.println("[CONFIG] No saved config found, using defaults");
        preferences.end();
        return false;
    }
    
    // Load lights count
    lightsCount = preferences.getUChar("lights_count", 0);
    Serial.printf("[CONFIG] Found %d lights in NVS\n", lightsCount);
    
    // Load each light
    for (uint8_t i = 0; i < lightsCount && i < MAX_LIGHTS; i++) {
        char key[16];
        sprintf(key, "light_%d", i);
        size_t lightLen = preferences.getBytes(key, &lights[i], sizeof(Light));
        if (lightLen == sizeof(Light)) {
            Serial.printf("[CONFIG] Loaded light %d: %s\n", i, lights[i].name);
        }
    }
    
    // Load groups count
    groupsCount = preferences.getUChar("groups_count", 1);
    
    // Load each group
    for (uint8_t i = 0; i < groupsCount && i < MAX_GROUPS; i++) {
        char key[16];
        sprintf(key, "group_%d", i);
        preferences.getBytes(key, &groups[i], sizeof(Group));
    }
    
    // Load API users count
    apiUsersCount = preferences.getUChar("users_count", 0);
    
    // Load each API user
    for (uint8_t i = 0; i < apiUsersCount && i < MAX_API_USERS; i++) {
        char key[16];
        sprintf(key, "user_%d", i);
        preferences.getBytes(key, &apiUsers[i], sizeof(ApiUser));
    }
    
    preferences.end();
    
    Serial.println("[CONFIG] Configuration loaded successfully");
    return true;
}

// Save configuration to NVS
bool saveConfig() {
    Serial.println("[CONFIG] Saving configuration to NVS...");
    
    if (!preferences.begin(NVS_NAMESPACE, false)) {
        Serial.println("[CONFIG] Failed to open NVS namespace");
        return false;
    }
    
    // Save bridge config
    preferences.putBytes(NVS_CONFIG, &bridgeConfig, sizeof(BridgeConfig));
    
    // Save lights count
    preferences.putUChar("lights_count", lightsCount);
    
    // Save each light
    for (uint8_t i = 0; i < lightsCount && i < MAX_LIGHTS; i++) {
        char key[16];
        sprintf(key, "light_%d", i);
        preferences.putBytes(key, &lights[i], sizeof(Light));
    }
    
    // Save groups count
    preferences.putUChar("groups_count", groupsCount);
    
    // Save each group
    for (uint8_t i = 0; i < groupsCount && i < MAX_GROUPS; i++) {
        char key[16];
        sprintf(key, "group_%d", i);
        preferences.putBytes(key, &groups[i], sizeof(Group));
    }
    
    // Save API users count
    preferences.putUChar("users_count", apiUsersCount);
    
    // Save each API user
    for (uint8_t i = 0; i < apiUsersCount && i < MAX_API_USERS; i++) {
        char key[16];
        sprintf(key, "user_%d", i);
        preferences.putBytes(key, &apiUsers[i], sizeof(ApiUser));
    }
    
    preferences.end();
    
    Serial.println("[CONFIG] Configuration saved successfully");
    return true;
}

// Load lights from NVS
bool loadLights() {
    return loadConfig(); // Lights are loaded as part of config
}

// Save lights to NVS
bool saveLights() {
    return saveConfig(); // Lights are saved as part of config
}
