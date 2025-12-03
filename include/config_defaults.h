#ifndef CONFIG_DEFAULTS_H
#define CONFIG_DEFAULTS_H

// ============================================================================
// CONFIGURABLE DEFAULTS
// These values can be modified to customize your Hue Bridge behavior
// ============================================================================

// -----------------------------------------------------------------------------
// Network Configuration
// -----------------------------------------------------------------------------

// WiFi Configuration Portal
#define WIFI_CONFIG_PORTAL_TIMEOUT 180  // Seconds (3 minutes)
#define WIFI_AP_NAME_PREFIX "Hue-"   // AP name will be "Hue-XXXXXX"

// -----------------------------------------------------------------------------
// NTP Time Synchronization
// -----------------------------------------------------------------------------

// Primary and secondary NTP servers
#define NTP_SERVER_PRIMARY "pool.ntp.org"
#define NTP_SERVER_SECONDARY "time.nist.gov"

// NTP sync timeout
#define NTP_SYNC_TIMEOUT_MS 10000       // 10 seconds
#define NTP_SYNC_RETRY_COUNT 20         // Number of retries

// Timezone offset (in seconds from UTC)
// Examples:
//   UTC: 0
//   EST: -18000 (UTC-5)
//   PST: -28800 (UTC-8)
//   IST: 19800 (UTC+5:30)
#define NTP_TIMEZONE_OFFSET 0           // UTC by default
#define NTP_DAYLIGHT_OFFSET 0           // Daylight saving offset

// -----------------------------------------------------------------------------
// Light Discovery Configuration
// -----------------------------------------------------------------------------

// Network scan range (IPs around the bridge to scan)
#define LIGHT_SCAN_RANGE 20             // Scan +/- 20 IPs around bridge

// Light detection timeout
#define LIGHT_DETECT_TIMEOUT_MS 100     // 100ms per light

// Periodic scan interval (disabled by default to prevent blocking)
#define LIGHT_SCAN_INTERVAL_MS 300000   // 5 minutes (if enabled)

// Light detection endpoint
#define LIGHT_DETECT_ENDPOINT "/detect"

// -----------------------------------------------------------------------------
// SSDP Discovery Configuration
// -----------------------------------------------------------------------------

// SSDP multicast address and port (standard UPnP values)
#define SSDP_MULTICAST_IP "239.255.255.250"
#define SSDP_MULTICAST_PORT 1900

// SSDP notify interval
#define SSDP_NOTIFY_INTERVAL_MS 30000   // 30 seconds

// SSDP cache control max-age
#define SSDP_CACHE_MAX_AGE 100          // Seconds

// -----------------------------------------------------------------------------
// Link Button Configuration
// -----------------------------------------------------------------------------

// Link button timeout (for pairing new apps)
#define LINK_BUTTON_TIMEOUT_MS 30000    // 30 seconds

// Auto-enable link button on first boot (no users)
#define LINK_BUTTON_AUTO_ENABLE true

// -----------------------------------------------------------------------------
// HTTPS/SSL Configuration
// -----------------------------------------------------------------------------

// Enable HTTPS server
#define HTTPS_ENABLED 1  // Enabled for official Hue App support

// HTTPS server port (standard is 443)
#define HTTPS_DEFAULT_PORT 443

// SSL/TLS timeout
#define HTTPS_CLIENT_TIMEOUT_MS 5000    // 5 seconds

// -----------------------------------------------------------------------------
// Manufacturer Information
// -----------------------------------------------------------------------------

// These values mimic the Philips Hue Bridge for compatibility
#define DEFAULT_MANUFACTURER "Philips"
#define DEFAULT_SW_VERSION "1948086000"  // Matches real Hue Bridge v2 format
#define DEFAULT_LIGHT_TYPE "Extended color light"
#define DEFAULT_LIGHT_MODEL "LST002"

// -----------------------------------------------------------------------------
// Light Default Capabilities
// -----------------------------------------------------------------------------

// Color temperature range (mireds)
#define LIGHT_CT_MIN 153                // Coolest (6500K)
#define LIGHT_CT_MAX 500                // Warmest (2000K)

// Default light state
#define LIGHT_DEFAULT_BRI 254           // Maximum brightness
#define LIGHT_DEFAULT_CT 200            // Neutral white
#define LIGHT_DEFAULT_HUE 0
#define LIGHT_DEFAULT_SAT 0

// -----------------------------------------------------------------------------
// Server Configuration
// -----------------------------------------------------------------------------

// Main loop delay (to keep watchdog happy)
#define MAIN_LOOP_DELAY_MS 10

// TCP/IP stack settle time after WiFi connection
#define NETWORK_SETTLE_DELAY_MS 2000

// -----------------------------------------------------------------------------
// Debug Configuration
// -----------------------------------------------------------------------------

// Enable verbose logging
#define DEBUG_VERBOSE false

// Serial baud rate
#define SERIAL_BAUD_RATE 115200

// Serial startup delay
#define SERIAL_STARTUP_DELAY_MS 1000

// -----------------------------------------------------------------------------
// mDNS Configuration
// -----------------------------------------------------------------------------

// mDNS hostname
#define MDNS_HOSTNAME "hue-bridge"

// -----------------------------------------------------------------------------
// Resource Limits
// -----------------------------------------------------------------------------

// These are defined in config.h but listed here for reference:
// MAX_LIGHTS = 50
// MAX_GROUPS = 16
// MAX_SCENES = 100
// MAX_SCHEDULES = 100
// MAX_SENSORS = 60
// MAX_RULES = 200
// MAX_API_USERS = 10

// -----------------------------------------------------------------------------
// Hardware Configuration
// -----------------------------------------------------------------------------

// Status LED (WS2812B)
#define STATUS_LED_PIN 21
#define STATUS_LED_COUNT 1
#define STATUS_LED_BRIGHTNESS 50        // 0-255

// LED Colors (R, G, B)
#define LED_COLOR_BOOT {0, 0, 255}      // Blue
#define LED_COLOR_IDLE {0, 255, 255}    // Teal
#define LED_COLOR_LINK {0, 255, 0}      // Green
#define LED_COLOR_ERROR {255, 0, 0}     // Red
#define LED_COLOR_STREAM {128, 0, 128}  // Purple

// Physical Link Button
#define LINK_BUTTON_PIN 0               // GPIO 0 (Boot Button)
#define LINK_BUTTON_ACTIVE_LOW true     // True if button pulls to GND

#endif // CONFIG_DEFAULTS_H
