/**
 * OTAA Basic Example
 *
 * This example shows how to use the OTAA library for automatic OTA updates.
 *
 * Circuit:
 *   - ESP32 or ESP8266 board
 *   - Connected to WiFi
 *
 * Created by OTAA Team
 * MIT License
 */

#include <WiFi.h>
#include <OTAA.h>

// WiFi credentials
const char* ssid = "your-wifi-ssid";
const char* password = "your-wifi-password";

// OTAA configuration
const char* serverUrl = "http://118.145.100.70";
const char* activationCode = "YOUR_ACTIVATION_CODE";  // Get from OTAA console

// Firmware version (change this when you release new firmware)
#define FIRMWARE_VERSION "1.0.0"

// Create OTAA instance
OTAA ota;

// State callback
void onOTAState(OTAState state) {
    switch (state) {
        case OTA_IDLE:
            Serial.println("[OTA] Idle");
            break;
        case OTA_CHECKING:
            Serial.println("[OTA] Checking for updates...");
            break;
        case OTA_UPDATE_AVAILABLE:
            Serial.println("[OTA] Update available!");
            break;
        case OTA_DOWNLOADING:
            Serial.println("[OTA] Downloading firmware...");
            break;
        case OTA_INSTALLING:
            Serial.println("[OTA] Installing firmware...");
            break;
        case OTA_SUCCESS:
            Serial.println("[OTA] Update successful! Restarting...");
            break;
        case OTA_FAILED:
            Serial.println("[OTA] Update failed: " + ota.getLastError());
            break;
    }
}

// Progress callback
void onOTAProgress(int progress, size_t downloaded, size_t total) {
    Serial.printf("[OTA] Progress: %d%% (%d/%d bytes)\n", progress, downloaded, total);
}

// Error callback
void onOTAError(const String& error) {
    Serial.println("[OTA] Error: " + error);
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n[OTAA] Basic Example");
    Serial.println("[OTAA] Firmware: v" + String(FIRMWARE_VERSION));

    // Connect to WiFi
    Serial.println("[WiFi] Connecting to " + String(ssid));
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\n[WiFi] Connected!");
    Serial.println("[WiFi] IP: " + WiFi.localIP().toString());

    // Initialize OTAA with activation code
    ota.setFirmwareVersion(FIRMWARE_VERSION);
    ota.onStateChange(onOTAState);
    ota.onProgress(onOTAProgress);
    ota.onError(onOTAError);

    if (ota.beginWithActivationCode(serverUrl, activationCode)) {
        Serial.println("[OTAA] Initialized successfully");
        Serial.println("[OTAA] Device ID: " + ota.getDeviceId());

        // Check for updates
        if (ota.checkUpdate()) {
            Serial.println("[OTAA] Update available, starting update...");
            ota.update();
        } else {
            Serial.println("[OTAA] Firmware is up to date");
        }
    } else {
        Serial.println("[OTAA] Initialization failed: " + ota.getLastError());
    }
}

void loop() {
    // Auto check for updates every 6 hours
    ota.autoCheck();

    // Your code here
    delay(1000);
}
