/**
 * OTAA With Display Example
 *
 * This example shows how to use the OTAA library with a display
 * to show update progress and status.
 *
 * Hardware:
 *   - ESP32-S3
 *   - SSD1306 OLED Display (I2C)
 *
 * Created by OTAA Team
 * MIT License
 */

#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OTAA.h>

// Display configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// WiFi credentials
const char* ssid = "your-wifi-ssid";
const char* password = "your-wifi-password";

// OTAA configuration
const char* serverUrl = "http://118.145.100.70";
const char* activationCode = "YOUR_ACTIVATION_CODE";

// Firmware version
#define FIRMWARE_VERSION "1.0.0"

// Create OTAA instance
OTAA ota;

// Display update flag
bool needDisplayUpdate = true;

void setup() {
    Serial.begin(115200);

    // Initialize display
    Wire.begin(8, 9);  // SDA=8, SCL=9 for ESP32-S3
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("[Display] Initialization failed!");
        while (1);
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("OTAA Updater");
    display.println("v" + String(FIRMWARE_VERSION));
    display.display();

    // Connect to WiFi
    Serial.println("[WiFi] Connecting to " + String(ssid));
    WiFi.begin(ssid, password);

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Connecting WiFi...");
    display.display();

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\n[WiFi] Connected!");
    display.println("Connected!");
    display.println(WiFi.localIP().toString());
    display.display();
    delay(1000);

    // Initialize OTAA
    ota.setFirmwareVersion(FIRMWARE_VERSION);
    ota.onStateChange(onOTAState);
    ota.onProgress(onOTAProgress);
    ota.onError(onOTAError);

    if (ota.beginWithActivationCode(serverUrl, activationCode)) {
        Serial.println("[OTAA] Initialized");
        displayStatus("OTAA Ready", "ID: " + ota.getDeviceId().substring(0, 12));
        delay(2000);

        // Check for updates
        checkForUpdate();
    } else {
        displayError("Init Failed", ota.getLastError());
    }
}

void loop() {
    // Auto check for updates every 6 hours
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck >= 6 * 60 * 60 * 1000) {
        lastCheck = millis();
        checkForUpdate();
    }

    // Update display if needed
    if (needDisplayUpdate) {
        needDisplayUpdate = false;
        displayStatus("Running", "v" + String(FIRMWARE_VERSION));
    }

    delay(1000);
}

void checkForUpdate() {
    displayStatus("Checking...", "Please wait");

    if (ota.checkUpdate()) {
        FirmwareInfo info = ota.getFirmwareInfo();
        displayStatus("Update Found!", "v" + info.version);

        Serial.println("[OTAA] Update available: v" + info.version);
        delay(2000);

        // Start update
        displayStatus("Updating...", "0%");
        ota.update();
    } else {
        displayStatus("Up to Date", "v" + String(FIRMWARE_VERSION));
        needDisplayUpdate = true;
    }
}

void onOTAState(OTAState state) {
    switch (state) {
        case OTA_CHECKING:
            displayStatus("Checking...", "Please wait");
            break;
        case OTA_UPDATE_AVAILABLE:
            displayStatus("Update Found!", "");
            break;
        case OTA_DOWNLOADING:
            displayStatus("Downloading...", "0%");
            break;
        case OTA_INSTALLING:
            displayStatus("Installing...", "Please wait");
            break;
        case OTA_SUCCESS:
            displayStatus("Success!", "Restarting...");
            break;
        case OTA_FAILED:
            displayError("Update Failed", ota.getLastError());
            break;
    }
}

void onOTAProgress(int progress, size_t downloaded, size_t total) {
    Serial.printf("[OTA] Progress: %d%%\n", progress);

    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.println("Downloading...");

    // Draw progress bar
    display.drawRect(0, 20, 128, 10, SSD1306_WHITE);
    display.fillRect(2, 22, (progress * 124) / 100, 6, SSD1306_WHITE);

    // Show percentage
    display.setCursor(0, 35);
    display.setTextSize(2);
    display.print(progress);
    display.println("%");

    // Show size
    display.setTextSize(1);
    display.setCursor(0, 55);
    display.print(String(downloaded / 1024) + "/" + String(total / 1024) + " KB");

    display.display();
}

void onOTAError(const String& error) {
    displayError("Error", error);
}

void displayStatus(const char* title, const String& message) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.println(title);
    display.println();
    display.setTextSize(1);
    display.println(message);
    display.display();
}

void displayError(const char* title, const String& error) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.println(title);
    display.println();

    // Wrap long error message
    String wrapped = error;
    while (wrapped.length() > 21) {
        display.println(wrapped.substring(0, 21));
        wrapped = wrapped.substring(21);
    }
    display.println(wrapped);

    display.display();
}
