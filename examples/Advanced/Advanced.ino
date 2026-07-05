/**
 * OTAA Advanced Example
 *
 * This example shows advanced usage of the OTAA library including:
 * - Manual update checking
 * - Custom update scheduling
 * - Status display
 * - Error handling
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
const char* deviceId = "esp32_xxxxxxxx";      // Your device ID
const char* deviceToken = "your-device-token"; // Your device token

// Firmware version
#define FIRMWARE_VERSION "1.0.0"

// Update check interval (milliseconds)
#define CHECK_INTERVAL 3600000  // 1 hour

// Create OTAA instance
OTAA ota;

// Timing variables
unsigned long lastCheckTime = 0;
unsigned long lastPrintTime = 0;

// State names for display
const char* stateNames[] = {
    "Idle",
    "Checking",
    "Update Available",
    "Downloading",
    "Installing",
    "Success",
    "Failed"
};

// State callback
void onOTAState(OTAState state) {
    Serial.println("\n========================================");
    Serial.println("[OTA] State changed: " + String(stateNames[state]));
    Serial.println("========================================\n");

    if (state == OTA_UPDATE_AVAILABLE) {
        FirmwareInfo info = ota.getFirmwareInfo();
        Serial.println("[OTA] New version: v" + info.version);
        Serial.println("[OTA] Size: " + String(info.fileSize) + " bytes");
        Serial.println("[OTA] Release notes: " + info.releaseNotes);
    }
}

// Progress callback
void onOTAProgress(int progress, size_t downloaded, size_t total) {
    // Print progress bar
    int barWidth = 30;
    int filled = (progress * barWidth) / 100;

    Serial.print("\r[OTA] [");
    for (int i = 0; i < barWidth; i++) {
        if (i < filled) {
            Serial.print("=");
        } else if (i == filled) {
            Serial.print(">");
        } else {
            Serial.print(" ");
        }
    }
    Serial.print("] " + String(progress) + "%");
    Serial.print(" (" + String(downloaded) + "/" + String(total) + " bytes)");

    if (progress == 100) {
        Serial.println();
    }
}

// Error callback
void onOTAError(const String& error) {
    Serial.println("\n[OTA] ERROR: " + error);
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n[OTAA] Advanced Example");
    Serial.println("[OTAA] Firmware: v" + String(FIRMWARE_VERSION));

    // Connect to WiFi
    Serial.println("[WiFi] Connecting to " + String(ssid));
    WiFi.begin(ssid, password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\n[WiFi] Connection failed!");
        Serial.println("[WiFi] Restarting...");
        ESP.restart();
    }

    Serial.println("\n[WiFi] Connected!");
    Serial.println("[WiFi] IP: " + WiFi.localIP().toString());
    Serial.println("[WiFi] RSSI: " + String(WiFi.RSSI()) + " dBm");

    // Initialize OTAA
    ota.setFirmwareVersion(FIRMWARE_VERSION);
    ota.setCheckInterval(CHECK_INTERVAL);
    ota.onStateChange(onOTAState);
    ota.onProgress(onOTAProgress);
    ota.onError(onOTAError);

    if (ota.begin(serverUrl, deviceId, deviceToken)) {
        Serial.println("[OTAA] Initialized successfully");
        Serial.println("[OTAA] Device ID: " + ota.getDeviceId());

        // Initial update check
        checkForUpdate();
    } else {
        Serial.println("[OTAA] Initialization failed!");
    }

    printSystemInfo();
}

void loop() {
    // Check for updates at interval
    if (millis() - lastCheckTime >= CHECK_INTERVAL) {
        checkForUpdate();
    }

    // Print status every 30 seconds
    if (millis() - lastPrintTime >= 30000) {
        lastPrintTime = millis();
        printStatus();
    }

    // Your application code here
    delay(100);
}

void checkForUpdate() {
    lastCheckTime = millis();

    Serial.println("\n[OTAA] Checking for updates...");
    Serial.println("[OTAA] Current version: v" + String(FIRMWARE_VERSION));

    if (ota.checkUpdate()) {
        Serial.println("[OTAA] Update available!");

        // Ask user for confirmation (in real app, you might auto-update)
        Serial.println("[OTAA] Starting update in 5 seconds...");
        Serial.println("[OTAA] Press 's' to skip, 'u' to update now");

        unsigned long startTime = millis();
        bool userCanceled = false;

        while (millis() - startTime < 5000) {
            if (Serial.available()) {
                char c = Serial.read();
                if (c == 's' || c == 'S') {
                    Serial.println("[OTAA] Update skipped by user");
                    userCanceled = true;
                    break;
                } else if (c == 'u' || c == 'U') {
                    break;
                }
            }
            delay(100);
        }

        if (!userCanceled) {
            ota.update();
        }
    } else {
        Serial.println("[OTAA] Firmware is up to date");
    }
}

void printStatus() {
    Serial.println("\n--- Status ---");
    Serial.println("[System] Uptime: " + String(millis() / 1000) + "s");
    Serial.println("[System] Free heap: " + String(ESP.getFreeHeap()) + " bytes");
    Serial.println("[WiFi] RSSI: " + String(WiFi.RSSI()) + " dBm");
    Serial.println("[WiFi] IP: " + WiFi.localIP().toString());
    Serial.println("[OTAA] State: " + String(stateNames[ota.getState()]));
    Serial.println("[OTAA] Last check: " + String((millis() - lastCheckTime) / 1000) + "s ago");
    Serial.println("--------------\n");
}

void printSystemInfo() {
    Serial.println("\n--- System Info ---");
    Serial.println("[System] Chip: " + String(ESP.getChipModel()));
    Serial.println("[System] Cores: " + String(ESP.getChipCores()));
    Serial.println("[System] Revision: " + String(ESP.getChipRevision()));
    Serial.println("[System] Flash: " + String(ESP.getFlashChipSize() / 1024 / 1024) + " MB");
    Serial.println("[System] PSRAM: " + String(ESP.getPsramSize() / 1024) + " KB");
    Serial.println("[System] SDK: " + String(ESP.getSdkVersion()));
    Serial.println("-------------------\n");
}
