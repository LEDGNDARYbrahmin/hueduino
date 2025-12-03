#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <time.h>
#include "config.h"
#include "config_defaults.h"
#include "discovery.h"
#include "hue_api.h"
#include "dashboard.h"
#include "https_server.h"
#include "ssl_cert.h"
#include "storage_manager.h"
#include "scenes_v2.h"
#include "entertainment.h"
#include "status_led.h"

// -----------------------------------------------------------------
// Global objects
// -----------------------------------------------------------------
WebServer server(DEFAULT_HTTP_PORT);
HTTPSServer httpsServer(HTTPS_DEFAULT_PORT);
SSDPDiscovery ssdp;
MDNSService mdns;
LightDiscovery lightDiscovery;
HueAPI hueAPI(&server);

// Advanced features (extern in their headers)
StorageManager storageManager;
// External declarations are already in headers or handled by the linker
// ensuring we don't have stray text

// -----------------------------------------------------------------
// Timing for periodic light scans
// -----------------------------------------------------------------
unsigned long lastLightScan = 0;

// -----------------------------------------------------------------
// Forward declarations
// -----------------------------------------------------------------
void setupWiFi();
void setupTime();
void setupWebServer();
void setupHTTPSServer();
void handleLinkButton();
String handleHTTPSRequest(const String& path, const String& method, const String& body);

// -----------------------------------------------------------------
// Setup â€“ runs once at boot
// -----------------------------------------------------------------
void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(SERIAL_STARTUP_DELAY_MS);

    // Initialize Status LED first
    statusLed.begin();
    statusLed.setState(LED_STATE_BOOT);

    Serial.println("\n\n");
    Serial.println("========================================");
    Serial.println("   diyHue Bridge - ESP32â€‘S3");
    Serial.println("   Version: " BRIDGE_VERSION);
    Serial.println("   Advanced Features Edition");
    Serial.println("========================================\n");

    // Initialize Link Button Pin
    pinMode(LINK_BUTTON_PIN, INPUT_PULLUP);

    // Seed RNG
    randomSeed(analogRead(0));

    // -----------------------------------------------------------------
    // Initialize Storage Manager (first, before loading config)
    // -----------------------------------------------------------------
    storageManager.begin();
    storageManager.enableAutoSave(true, 5000); // 5 second debounce
    
    // Initialize namespaces to prevent "nvs_open failed" errors
    Preferences prefs;
    if (prefs.begin("scenes", false)) { 
        if (!prefs.isKey("init")) prefs.putBool("init", true);
        prefs.end(); 
    }
    if (prefs.begin("entertainment", false)) { 
        if (!prefs.isKey("init")) prefs.putBool("init", true);
        prefs.end(); 
    }
    
    Serial.println("[SETUP] Storage Manager initialized");

    // Load / create configuration
    initConfig();
    if (!loadConfig()) {
        Serial.println("[SETUP] No saved configuration, using defaults");
        saveConfig();
    } else {
        Serial.println("[SETUP] Configuration loaded successfully");
        
        // Force update versions to match firmware
        bool versionChanged = false;
        if (strcmp(bridgeConfig.swversion, BRIDGE_SW_VERSION) != 0) {
            Serial.printf("[SETUP] Updating swversion: %s -> %s\n", bridgeConfig.swversion, BRIDGE_SW_VERSION);
            strcpy(bridgeConfig.swversion, BRIDGE_SW_VERSION);
            versionChanged = true;
        }
        if (strcmp(bridgeConfig.apiversion, BRIDGE_API_VERSION) != 0) {
            Serial.printf("[SETUP] Updating apiversion: %s -> %s\n", bridgeConfig.apiversion, BRIDGE_API_VERSION);
            strcpy(bridgeConfig.apiversion, BRIDGE_API_VERSION);
            versionChanged = true;
        }
        
        if (versionChanged) {
            saveConfig();
        }

        Serial.printf("[SETUP] Bridge ID: %s\n", bridgeConfig.bridgeid);
        Serial.printf("[SETUP] Lights count: %d\n", lightsCount);
    }

    // -----------------------------------------------------------------
    // Network stack initialisation
    // -----------------------------------------------------------------
    statusLed.setState(LED_STATE_WIFI_CONFIG);
    setupWiFi();
    statusLed.setState(LED_STATE_BOOT); // Back to boot animation while syncing time
    setupTime();

    // Give the TCP/IP core a moment to settle
    delay(NETWORK_SETTLE_DELAY_MS);

    // -----------------------------------------------------------------
    // Start web servers
    // -----------------------------------------------------------------
    server.begin();
    Serial.printf("[SETUP] HTTP server started on port %d\n", DEFAULT_HTTP_PORT);

    #if HTTPS_ENABLED
    setupHTTPSServer();
    #else
    Serial.println("[SETUP] HTTPS server disabled in configuration");
    #endif

    // -----------------------------------------------------------------
    // Discovery services
    // -----------------------------------------------------------------
    mdns.begin(MDNS_HOSTNAME);
    Serial.println("[SETUP] mDNS started");
    
    ssdp.begin();
    Serial.println("[SETUP] SSDP started");

    // -----------------------------------------------------------------
    // Register UI routes
    // -----------------------------------------------------------------
    setupWebServer();

    // -----------------------------------------------------------------
    // Initialise Hue API
    // -----------------------------------------------------------------
    hueAPI.begin();
    
    // Register custom handler for /api routes to prevent "handler not found" errors
    server.addHandler(new HueRequestHandler(&hueAPI));
    
    server.onNotFound([]() {
        // Only log non-API 404s
        if (!server.uri().startsWith("/api")) {
            Serial.printf("[HTTP] 404 Not Found: %s %s\n", server.method() == HTTP_GET ? "GET" : "POST", server.uri().c_str());
            server.send(404, "text/plain", "Not found");
        }
    });

    // -----------------------------------------------------------------
    // Initialize Advanced Features
    // -----------------------------------------------------------------
    Serial.println("\n[SETUP] Initializing advanced features...");
    
    // Scene Manager
    sceneManager.begin();
    Serial.printf("[SETUP] Scene Manager: %d scenes loaded\n", sceneManager.getScenesCount());
    
    // Entertainment Manager
    entertainmentManager.begin();
    Serial.printf("[SETUP] Entertainment Manager: %d areas loaded\n", entertainmentManager.getAreasCount());

    // -----------------------------------------------------------------
    // Final boot messages
    // -----------------------------------------------------------------
    Serial.println("\n========================================");
    Serial.println("   Bridge Ready!");
    Serial.printf("   IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("   MAC Address: %s\n", WiFi.macAddress().c_str());
    Serial.printf("   Bridge ID: %s\n", bridgeConfig.bridgeid);
    Serial.printf("   HTTP Port: %d\n", bridgeConfig.http_port);
    #if HTTPS_ENABLED
    Serial.printf("   HTTPS Port: %d\n", bridgeConfig.https_port);
    #endif
    Serial.printf("   Scenes: %d\n", sceneManager.getScenesCount());
    Serial.printf("   Entertainment Areas: %d\n", entertainmentManager.getAreasCount());
    Serial.println("========================================\n");

    // Enable link button for first boot if no users exist
    #if LINK_BUTTON_AUTO_ENABLE
    if (apiUsersCount == 0) {
        Serial.printf("[SETUP] No API users found, enabling link button for %d seconds\n", 
                      LINK_BUTTON_TIMEOUT_MS / 1000);
        bridgeConfig.linkbutton = true;
        bridgeConfig.linkbutton_timestamp = millis();
        statusLed.setState(LED_STATE_LINK);
    } else {
        statusLed.setState(LED_STATE_IDLE);
    }
    #else
    statusLed.setState(LED_STATE_IDLE);
    #endif

    // Schedule first light scan
    lastLightScan = millis() - LIGHT_SCAN_INTERVAL_MS + 10000;
}

// -----------------------------------------------------------------
// Main loop â€“ runs repeatedly
// -----------------------------------------------------------------
void loop() {
    // Handle incoming HTTP requests
    server.handleClient();

    // Handle incoming HTTPS requests
    #if HTTPS_ENABLED
    httpsServer.handleClient();
    #endif

    // SSDP discovery
    ssdp.handleClient();

    // Linkâ€‘button handling (Physical + Virtual)
    handleLinkButton();

    // -----------------------------------------------------------------
    // Advanced Features Updates
    // -----------------------------------------------------------------
    
    // Update storage manager (auto-save)
    storageManager.update();

    // Update dynamic scenes
    sceneManager.updateDynamicScenes();

    // Handle entertainment UDP packets
    entertainmentManager.handleUDP();
    
    // Update Status LED
    statusLed.update();

    // Check entertainment state for LED
    static bool wasStreaming = false;
    bool isStreaming = false;
    for(int i=0; i<entertainmentManager.getAreasCount(); i++) {
        if(entertainmentManager.getAreaByIndex(i)->streaming) {
            isStreaming = true;
            break;
        }
    }
    
    if (isStreaming && !wasStreaming) {
        statusLed.setState(LED_STATE_STREAM);
        wasStreaming = true;
    } else if (!isStreaming && wasStreaming) {
        statusLed.setState(LED_STATE_IDLE);
        wasStreaming = false;
    }

    // Small delay to keep the watchdog happy
    delay(MAIN_LOOP_DELAY_MS);
}

// -----------------------------------------------------------------
// Wiâ€‘Fi configuration (uses WiFiManager)
// -----------------------------------------------------------------
void setupWiFi() {
    Serial.println("[WiFi] Starting WiFi configuration...");

    WiFiManager wifiManager;
    String apName = String(WIFI_AP_NAME_PREFIX) + String(bridgeConfig.bridgeid).substring(6);
    wifiManager.setConfigPortalTimeout(WIFI_CONFIG_PORTAL_TIMEOUT);

    if (!wifiManager.autoConnect(apName.c_str())) {
        Serial.println("[WiFi] Failed to connect, restarting...");
        statusLed.setState(LED_STATE_ERROR);
        delay(3000);
        ESP.restart();
    }

    Serial.println("[WiFi] Connected successfully!");
    Serial.printf("[WiFi] IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WiFi] Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
    Serial.printf("[WiFi] Subnet: %s\n", WiFi.subnetMask().toString().c_str());

    // Store network info in NVS
    bridgeConfig.ipaddress = WiFi.localIP();
    bridgeConfig.netmask   = WiFi.subnetMask();
    bridgeConfig.gateway   = WiFi.gatewayIP();

    // Regenerate bridge ID to force a new identity
    // We use a random suffix to ensure it's different from any cached ID
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    String bridgeid = mac.substring(0, 6) + "FFFE" + mac.substring(6);
    // Add random offset to last byte to force change if needed
    // bridgeid[11] = "0123456789ABCDEF"[random(16)]; 
    
    strcpy(bridgeConfig.bridgeid, bridgeid.c_str());

    saveConfig();
}

// -----------------------------------------------------------------
// Time synchronisation via NTP
// -----------------------------------------------------------------
void setupTime() {
    Serial.println("[TIME] Configuring time...");

    configTime(NTP_TIMEZONE_OFFSET, NTP_DAYLIGHT_OFFSET, 
               NTP_SERVER_PRIMARY, NTP_SERVER_SECONDARY);

    int retry = 0;
    while (time(nullptr) < 100000 && ++retry < NTP_SYNC_RETRY_COUNT) {
        Serial.print(".");
        delay(NTP_SYNC_TIMEOUT_MS / NTP_SYNC_RETRY_COUNT);
    }
    Serial.println();

    if (retry < NTP_SYNC_RETRY_COUNT) {
        Serial.println("[TIME] Time synchronized");
        time_t now = time(nullptr);
        Serial.println(ctime(&now));
    } else {
        Serial.println("[TIME] Failed to sync time, continuing anyway...");
    }
}

// -----------------------------------------------------------------
// Register UI routes (Synchronous WebServer API)
// -----------------------------------------------------------------
void setupWebServer() {
    // Root page – handled by dashboard module
    setupDashboard(&server);

    // Link-button endpoint
    server.on("/linkbutton", HTTP_GET, []() {
        bridgeConfig.linkbutton = true;
        bridgeConfig.linkbutton_timestamp = millis();
        statusLed.setState(LED_STATE_LINK);
        Serial.printf("[LINK] Link button enabled for %d seconds\n", 
                      LINK_BUTTON_TIMEOUT_MS / 1000);
        server.send(200, "text/plain", "Link button enabled");
    });
    
    // Fake updater endpoint to prevent app update prompts
    server.on("/updater", HTTP_POST, []() {
        Serial.println("[UPDATER] App requested update - returning success (no-op)");
        server.send(200, "application/json", "{\"success\":true}");
    });

    // Manual light-scan endpoint
    server.on("/scan", HTTP_GET, []() {
        Serial.println("[SCAN] Manual light scan requested");
        lightDiscovery.scanNetwork();
        server.sendHeader("Location", "/");
        server.send(302, "text/plain", "Redirecting...");
    });
    
    // Manual add light endpoint (for ESP8266 at 10.93.137.190)
    server.on("/addlight", HTTP_GET, []() {
        if (lightsCount < MAX_LIGHTS) {
            IPAddress lightIP(10, 93, 137, 190);
            Light& light = lights[lightsCount];
            light.ip = lightIP;
            strcpy(light.name, "ESP8266 Light");
            strcpy(light.type, "Extended color light");
            strcpy(light.modelid, "LCT015");
            strcpy(light.manufacturername, "Philips");
            snprintf(light.uniqueid, sizeof(light.uniqueid), "00:17:88:01:00:%02x:%02x:%02x-0b",
                     lightIP[1], lightIP[2], lightIP[3]);
            strcpy(light.swversion, "1.0.0");
            light.on = false;
            light.bri = 254;
            light.hue = 0;
            light.sat = 0;
            light.ct = 366;
            light.xy[0] = 0.0;
            light.xy[1] = 0.0;
            light.colormode = 2;
            light.reachable = true;
            lightsCount++;
            saveConfig();
            Serial.printf("[LIGHT] Manually added light at %s\n", lightIP.toString().c_str());
            server.send(200, "text/plain", "Light added successfully!");
        } else {
            server.send(500, "text/plain", "Max lights reached");
        }
    });
    
    // Reset config endpoint (updates swversion)
    server.on("/resetconfig", HTTP_GET, []() {
        strcpy(bridgeConfig.swversion, DEFAULT_SW_VERSION);
        saveConfig();
        Serial.println("[CONFIG] Updated swversion to: " + String(DEFAULT_SW_VERSION));
        server.send(200, "text/plain", "Config updated! New swversion: " + String(DEFAULT_SW_VERSION));
    });
    
    // Scene test endpoint
    server.on("/scenes/test", HTTP_GET, []() {
        String response = "Scenes: " + String(sceneManager.getScenesCount()) + "\n";
        response += "Entertainment Areas: " + String(entertainmentManager.getAreasCount());
        server.send(200, "text/plain", response);
    });
}

// Setup HTTPS Server
// -----------------------------------------------------------------
void setupHTTPSServer() {
    Serial.println("[HTTPS] Initializing HTTPS server...");
    
    // Initialize SSL certificates
    initSSLCertificates();
    
    // Start HTTPS server with embedded certificate
    // Note: server_cert and server_key are defined in ssl_cert.h
    if (httpsServer.begin(server_cert, server_key)) {
        Serial.printf("[HTTPS] HTTPS server started on port %d\n", HTTPS_DEFAULT_PORT);
        
        // Set request handler
        httpsServer.setRequestHandler(handleHTTPSRequest);
        
        Serial.println("[HTTPS] HTTPS server ready");
    } else {
        Serial.println("[HTTPS] ERROR: Failed to start HTTPS server");
        Serial.println("[HTTPS] Check SSL certificate and available memory");
    }
}

// -----------------------------------------------------------------
// Handle Physical Link Button
// -----------------------------------------------------------------
void handleLinkButton() {
    static int lastButtonState = HIGH;
    static unsigned long buttonPressTime = 0;
    static bool longPressHandled = false;
    static unsigned long debounceDelay = 50;
    
    // Read physical button (Active LOW)
    int reading = digitalRead(LINK_BUTTON_PIN);
    
    // Button pressed (falling edge)
    if (reading == LOW && lastButtonState == HIGH) {
        buttonPressTime = millis();
        longPressHandled = false;
        Serial.println("[LINK] Button pressed");
    }
    
    // Button held down
    if (reading == LOW && !longPressHandled) {
        if (millis() - buttonPressTime > 5000) {
            // Long press detected (> 5 seconds)
            Serial.println("[LINK] Long press detected - FACTORY RESET");
            
            // Clear all users
            apiUsersCount = 0;
            saveConfig();
            
            // Blink LED rapidly
            for(int i=0; i<10; i++) {
                statusLed.setState(LED_STATE_ERROR);
                statusLed.update();
                delay(100);
                statusLed.setState(LED_STATE_IDLE);
                statusLed.update();
                delay(100);
            }
            
            Serial.println("[LINK] Factory reset complete - All users cleared");
            longPressHandled = true; // Prevent multiple triggers
        }
    }
    
    // Button released (rising edge)
    if (reading == HIGH && lastButtonState == LOW) {
        if (!longPressHandled) {
            // Short press - Enable Link Mode
            if (millis() - buttonPressTime > debounceDelay) {
                if (!bridgeConfig.linkbutton) {
                    bridgeConfig.linkbutton = true;
                    bridgeConfig.linkbutton_timestamp = millis();
                    statusLed.setState(LED_STATE_LINK);
                    Serial.println("[LINK] Physical button pressed - Link mode enabled");
                }
            }
        }
    }
    
    lastButtonState = reading;

    // Handle timeout
    if (bridgeConfig.linkbutton && 
        (millis() - bridgeConfig.linkbutton_timestamp > LINK_BUTTON_TIMEOUT_MS)) {
        bridgeConfig.linkbutton = false;
        statusLed.setState(LED_STATE_IDLE);
        Serial.println("[LINK] Link button disabled (timeout)");
    }
}

// -----------------------------------------------------------------
// Handle HTTPS Requests (Discovery & Pairing)
// -----------------------------------------------------------------
String handleHTTPSRequest(const String& path, const String& method, const String& body) {
    // 1. Description XML (Discovery)
    if (path == "/description.xml" && method == "GET") {
        return hueAPI.getDescriptionXML();
    }

    // 2. User Creation (Pairing)
    // The Hue App sends POST /api with {"devicetype":"my_app#iphone"}
    if ((path == "/api" || path == "/api/") && method == "POST") {
        if (bridgeConfig.linkbutton) {
            // Generate new user
            String username = hueAPI.generateUsername();
            
            // Save user to NVS
            if (apiUsersCount < 10) {
                strncpy(apiUsers[apiUsersCount].username, username.c_str(), 63);
                apiUsers[apiUsersCount].username[63] = '\0';
                strncpy(apiUsers[apiUsersCount].devicetype, "HueApp", 63);
                apiUsers[apiUsersCount].devicetype[63] = '\0';
                snprintf(apiUsers[apiUsersCount].create_date, 32, "%lu", millis());
                apiUsersCount++;
                saveConfig();
                Serial.printf("[HTTPS] Pairing success! Created & saved user: %s\n", username.c_str());
            } else {
                Serial.println("[HTTPS] Max users reached!");
            }
            
            // Return success JSON array
            return "[{\"success\":{\"username\":\"" + username + "\"}}]";
        } else {
            Serial.println("[HTTPS] Pairing failed: Link button not pressed");
            return "[{\"error\":{\"type\":101,\"address\":\"\",\"description\":\"link button not pressed\"}}]";
        }
    }
    
    
    // 3. Basic Config (optional, for apps that check config via HTTPS)
    if (path.startsWith("/api/") && path.endsWith("/config") && method == "GET") {
        // Extract username if needed, but for config we might just return the standard config
        // For simplicity, return a minimal config or delegate to HueAPI if possible
        // But HueAPI methods usually take arguments.
        // Let's return a basic JSON
        return "{\"name\":\"Philips Hue\",\"datastoreversion\":\"90\",\"swversion\":\"" BRIDGE_SW_VERSION "\",\"apiversion\":\"" BRIDGE_API_VERSION "\",\"mac\":\"" + WiFi.macAddress() + "\",\"bridgeid\":\"" + WiFi.macAddress() + "\",\"factorynew\":false,\"replacesbridgeid\":null,\"modelid\":\"BSB002\",\"starterkitid\":\"\"}";
    }

    // Default 404 response (handled by caller if we return empty, but we return empty JSON)
    return "{}"; 
}
