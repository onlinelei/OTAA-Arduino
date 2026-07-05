# OTAA Arduino Library

[![Arduino Library](https://www.ardu-badge.com/badge/OTAA.svg)](https://www.ardu-badge.com/OTAA)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-Library-orange.svg)](https://platformio.org/lib/show/OTAA)
[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

Easy-to-use OTA (Over-The-Air) update library for ESP32 and ESP8266 devices. Connects to OTAA server for automatic firmware updates.

## Features

- ✅ Automatic firmware updates
- ✅ Progress tracking with callbacks
- ✅ MD5 verification
- ✅ Support ESP32 and ESP8266
- ✅ Arduino IDE and PlatformIO support
- ✅ Activation code based device registration
- ✅ Auto check for updates
- ✅ Detailed error reporting

## Installation

### Arduino IDE

1. Open Arduino IDE
2. Go to **Sketch → Include Library → Manage Libraries**
3. Search for "**OTAA**"
4. Click **Install**

### PlatformIO

Add to your `platformio.ini`:

```ini
lib_deps =
    otaa/OTAA@^1.0.0
```

Or install via CLI:

```bash
pio pkg install --library "otaa/OTAA"
```

### Manual Installation

1. Download the latest release from [GitHub](https://github.com/otaa-platform/OTAA-Arduino/releases)
2. Extract to `~/Arduino/libraries/OTAA`
3. Restart Arduino IDE

## Quick Start

### 1. Get Activation Code

1. Visit [OTAA Console](http://118.145.100.70)
2. Register an account
3. Go to "激活码管理" (Activation Codes)
4. Click "生成激活码" (Generate)
5. Copy the activation code

### 2. Write Your Code

```cpp
#include <WiFi.h>
#include <OTAA.h>

// Your WiFi credentials
const char* ssid = "your-wifi-ssid";
const char* password = "your-wifi-password";

// OTAA configuration
const char* serverUrl = "http://118.145.100.70";
const char* activationCode = "YOUR_ACTIVATION_CODE";

// Your firmware version
#define FIRMWARE_VERSION "1.0.0"

OTAA ota;

void setup() {
    Serial.begin(115200);

    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected!");

    // Initialize OTAA
    ota.setFirmwareVersion(FIRMWARE_VERSION);

    if (ota.beginWithActivationCode(serverUrl, activationCode)) {
        Serial.println("OTAA initialized!");

        // Check for updates
        if (ota.checkUpdate()) {
            Serial.println("Update available, updating...");
            ota.update();
        }
    }
}

void loop() {
    // Auto check for updates every 6 hours
    ota.autoCheck();

    // Your code here
    delay(1000);
}
```

### 3. Upload and Test

1. Upload the code to your ESP32/ESP8266
2. Open Serial Monitor
3. The device will:
   - Connect to WiFi
   - Register with OTAA server
   - Check for updates
   - Download and install if available

## API Reference

### Initialization

```cpp
// Using activation code (recommended for first time)
bool beginWithActivationCode(const char* serverUrl, const char* activationCode);

// Using device ID and token (for registered devices)
bool begin(const char* serverUrl, const char* deviceId, const char* deviceToken);
```

### Update Control

```cpp
// Check for updates
bool checkUpdate();

// Download and install update
bool update();

// Auto check in loop()
bool autoCheck();
```

### Configuration

```cpp
// Set firmware version
void setFirmwareVersion(const char* version);

// Set auto check interval (default: 6 hours)
void setCheckInterval(unsigned long intervalMs);

// Enable/disable auto check
void setAutoCheck(bool enable);
```

### Callbacks

```cpp
// State change callback
void onStateChange(void (*callback)(OTAState state));

// Progress callback
void onProgress(void (*callback)(int progress, size_t downloaded, size_t total));

// Error callback
void onError(void (*callback)(const String& error));
```

### Getters

```cpp
OTAState getState();          // Get current state
int getProgress();            // Get download progress (0-100)
String getLastError();        // Get last error message
FirmwareInfo getFirmwareInfo(); // Get firmware info
String getDeviceId();         // Get device ID
String getDeviceToken();      // Get device token
```

## OTA States

| State | Description |
|-------|-------------|
| `OTA_IDLE` | Idle, no operation |
| `OTA_CHECKING` | Checking for updates |
| `OTA_UPDATE_AVAILABLE` | Update available |
| `OTA_DOWNLOADING` | Downloading firmware |
| `OTA_INSTALLING` | Installing firmware |
| `OTA_SUCCESS` | Update successful |
| `OTA_FAILED` | Update failed |

## Examples

- [Basic](examples/Basic/Basic.ino) - Simple OTA update
- [Advanced](examples/Advanced/Advanced.ino) - Advanced features with callbacks

## Releasing New Firmware

1. Update `FIRMWARE_VERSION` in your code
2. Build and upload to your device
3. Upload the new firmware binary to OTAA server:
   - Go to [OTAA Console](http://118.145.100.70)
   - Go to "固件中心" (Firmware Center)
   - Click "上传固件" (Upload Firmware)
   - Select your device type and upload the `.bin` file
4. All devices will automatically update on next check

## Troubleshooting

### Update not found

- Make sure `FIRMWARE_VERSION` is correct
- Check that the firmware is uploaded to OTAA server
- Verify device is registered (check device ID in console)

### Download fails

- Check WiFi connection
- Verify server URL is correct
- Check Serial Monitor for error messages

### Installation fails

- Ensure firmware binary is for the correct chip
- Check available flash space
- Verify MD5 checksum

## License

MIT License - see [LICENSE](LICENSE) file

## Links

- [OTAA Platform](http://118.145.100.70)
- [GitHub Repository](https://github.com/otaa-platform/OTAA-Arduino)
- [Issues](https://github.com/otaa-platform/OTAA-Arduino/issues)
