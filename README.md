# Hueduino - Open Source Philips Hue Bridge Clone

[![Platform](https://img.shields.io/badge/platform-ESP32--S3-blue.svg)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Framework](https://img.shields.io/badge/framework-Arduino-00979D.svg)](https://www.arduino.cc/)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-compatible-orange.svg)](https://platformio.org/)

A fully-featured **Philips Hue Bridge emulator** built on ESP32-S3 using the Arduino framework. Control smart lights, create scenes, and enjoy entertainment sync—all without proprietary hardware!

---

## Features

### Core Functionality
- **Full Hue API v1 Compatibility** - Works with official Philips Hue apps and third-party integrations
- **HTTPS Support** - Secure communication with SSL/TLS encryption for official app pairing
- **Automatic Discovery** - SSDP and mDNS for seamless device discovery
- **WiFi Configuration Portal** - Easy setup via captive portal (no hardcoded credentials)
- **Persistent Storage** - NVS-based configuration with auto-save functionality

### Light Management
- **Up to 50 Lights** - Control multiple smart lights simultaneously
- **Full Color Control** - RGB, HSV, XY color space, and color temperature
- **Automatic Light Discovery** - Network scanning to find compatible lights
- **Manual Light Addition** - Add lights via web dashboard
- **Light Groups** - Organize lights into rooms and zones (up to 16 groups)

### Advanced Features
- **Scene Manager** - Create and manage up to 100 custom scenes
- **Entertainment Mode** - High-speed UDP streaming for Hue Sync (screen mirroring, gaming, music visualization)
- **Dynamic Scenes** - Animated color transitions and effects
- **Schedules & Timers** - Automate lighting based on time (up to 100 schedules)
- **Rules Engine** - Create custom automation rules (up to 200 rules)
- **Sensors Support** - Motion sensors, switches, and more (up to 60 sensors)

### Hardware Features
- **Status LED** - WS2812B RGB LED for visual feedback
  - 🔵 Blue: Booting
  - 🟢 Green: Link mode (pairing)
  - 🔴 Red: Error
  - 🟣 Purple: Entertainment streaming
  - 🩵 Teal: Idle/Ready
- **Physical Link Button** - GPIO 0 (Boot button) for pairing
  - Short press: Enable pairing mode (30 seconds)
  - Long press (5s): Factory reset

### Web Dashboard
- Real-time bridge status monitoring
- Light management interface
- Manual light scanning
- Configuration management
- Scene testing

---

## Requirements

### Hardware
- **ESP32-S3 DevKit** (or compatible board)
- **WS2812B RGB LED** (optional, for status indication)
- **WiFi Network** (2.4 GHz)

### Software
- [PlatformIO](https://platformio.org/) (recommended) or Arduino IDE
- ESP32 Arduino Core v2.x or higher

---

## 🚀 Quick Start

### 1. Clone the Repository
```bash
git clone https://github.com/LEDGNDARYbrahmin/hueduino.git
cd hueduino
```

### 2. Install Dependencies
Using PlatformIO (recommended):
```bash
pio lib install
```

### 3. Configure Hardware (Optional)
Edit `include/config_defaults.h` to customize:
- Status LED pin (default: GPIO 21)
- Link button pin (default: GPIO 0)
- Network settings
- Light discovery range

### 4. Build and Upload
```bash
pio run --target upload
```

Or using Arduino IDE:
1. Open `src/main.cpp`
2. Select **ESP32-S3 Dev Module** as board
3. Click **Upload**

### 5. Initial Setup
1. Power on the ESP32-S3
2. Connect to WiFi network `Hue-XXXXXX` (captive portal)
3. Enter your WiFi credentials
4. Device will restart and connect to your network

### 6. Pair with Hue App
1. Open the official Philips Hue app
2. Search for bridges
3. Press the **Boot button** on ESP32 (or visit `http://<bridge-ip>/linkbutton`)
4. Complete pairing in the app

---

## API Endpoints

### Discovery
- `GET /description.xml` - UPnP device description
- SSDP multicast on `239.255.255.250:1900`
- mDNS as `hue-bridge.local`

### Hue API v1
- `POST /api` - Create new user (pairing)
- `GET /api/<username>` - Get full bridge state
- `GET /api/<username>/lights` - List all lights
- `PUT /api/<username>/lights/<id>/state` - Control light
- `GET /api/<username>/groups` - List all groups
- `PUT /api/<username>/groups/<id>/action` - Control group
- `GET /api/<username>/scenes` - List all scenes
- `GET /api/<username>/config` - Bridge configuration

### Web Dashboard
- `GET /` - Dashboard home
- `GET /linkbutton` - Enable pairing mode
- `GET /scan` - Scan for new lights
- `GET /addlight` - Manually add light

### Entertainment
- UDP port `2100` - Entertainment streaming (Hue Sync)

---

## Entertainment Mode (Hue Sync)

Entertainment mode enables real-time, low-latency color streaming for:
- **Screen Mirroring** - Sync lights with your TV/monitor
- **Gaming** - Immersive lighting effects
- **Music Visualization** - Lights dance to the beat
- **Ambilight** - Ambient lighting effects

**Protocol**: UDP on port 2100  
**Update Rate**: Up to 60 Hz  
**Latency**: < 20ms

---

## Configuration

### Default Settings
All configurable values are in `include/config_defaults.h`:

| Setting | Default | Description |
|---------|---------|-------------|
| `WIFI_CONFIG_PORTAL_TIMEOUT` | 180s | WiFi setup timeout |
| `LIGHT_SCAN_RANGE` | ±20 IPs | Network scan range |
| `LINK_BUTTON_TIMEOUT_MS` | 30s | Pairing mode duration |
| `STATUS_LED_PIN` | GPIO 21 | WS2812B LED pin |
| `LINK_BUTTON_PIN` | GPIO 0 | Physical button pin |
| `MAX_LIGHTS` | 50 | Maximum lights |
| `MAX_GROUPS` | 16 | Maximum groups |
| `MAX_SCENES` | 100 | Maximum scenes |

### Network Configuration
The bridge automatically:
- Generates a unique Bridge ID from MAC address
- Obtains IP via DHCP
- Syncs time via NTP (`pool.ntp.org`)
- Announces itself via SSDP/mDNS

---

## Project Structure

```
hueduino/
├── src/
│   ├── main.cpp              # Main application entry point
│   ├── config.cpp            # Configuration management
│   ├── hue_api.cpp           # Hue API v1 implementation
│   ├── discovery.cpp         # SSDP/mDNS discovery
│   ├── dashboard.cpp         # Web dashboard UI
│   ├── https_server.cpp      # HTTPS server with SSL
│   ├── entertainment.cpp     # Entertainment mode (Hue Sync)
│   ├── scenes_v2.cpp         # Scene manager
│   ├── status_led.cpp        # Status LED controller
│   ├── storage_manager.cpp   # NVS persistence
│   └── ssl_cert.cpp          # SSL certificate management
├── include/
│   ├── config.h              # Configuration structures
│   ├── config_defaults.h     # Default configuration values
│   ├── hue_api.h             # API definitions
│   ├── discovery.h           # Discovery service definitions
│   ├── entertainment.h       # Entertainment mode definitions
│   └── ...
├── platformio.ini            # PlatformIO configuration
├── LICENSE                   # MIT License
└── README.md                 # This file
```

---

## Compatible Lights

This bridge works with any light that implements the diyHue protocol:
- **ESP8266/ESP32** lights with custom firmware
- **Arduino-based** smart lights
- **diyHue** compatible devices
- Lights with `/detect` endpoint for discovery

### Example Light Firmware
Check out compatible light firmware in the `light_firmwares/` folder (if available) or visit [diyHue](https://diyhue.org/) for more information.

---

## Troubleshooting

### Bridge Not Discovered
1. Ensure bridge and phone are on the same WiFi network
2. Check firewall settings (allow UDP port 1900 for SSDP)
3. Try manual IP entry in the Hue app
4. Restart the bridge and app

### Pairing Fails
1. Press the physical button or visit `/linkbutton` endpoint
2. Ensure link mode is active (green LED)
3. Complete pairing within 30 seconds
4. Check serial monitor for errors

### Lights Not Found
1. Run manual scan via `/scan` endpoint
2. Check light firmware has `/detect` endpoint
3. Verify lights are on the same network
4. Adjust `LIGHT_SCAN_RANGE` in config

### Factory Reset
1. Press and hold the Boot button for 5+ seconds
2. LED will blink red/teal rapidly
3. All users and pairings will be cleared
4. Bridge will restart

---

## Serial Monitor Output

Enable debug output at 115200 baud to see:
- Boot sequence and initialization
- WiFi connection status
- API requests and responses
- Light discovery results
- Entertainment streaming stats
- Error messages and warnings

---

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for:
- Bug fixes
- New features
- Documentation improvements
- Hardware compatibility

---

## 📜 License

This project is licensed under the **APACHE 2.0 License** - see the [LICENSE](LICENSE) file for details.

---

## Acknowledgments

- [diyHue](https://diyhue.org/) - Inspiration and protocol documentation
- [Philips Hue](https://www.philips-hue.com/) - Original Hue Bridge API
- [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32) - ESP32 support
- [ArduinoJson](https://arduinojson.org/) - JSON parsing library
- [WiFiManager](https://github.com/tzapu/WiFiManager) - WiFi configuration portal

---

## Support

- **GitHub Issues**: [Report bugs or request features](https://github.com/LEDGNDARYbrahmin/hueduino/issues)
- **Discussions**: [Ask questions and share ideas](https://github.com/LEDGNDARYbrahmin/hueduino/discussions)

---

## 🌟 Star History

If you find this project useful, please consider giving it a ⭐ on GitHub!

---
