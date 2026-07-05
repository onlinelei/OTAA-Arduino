/**
 * OTAA - OTA Update Library for Arduino
 *
 * Implementation file
 *
 * MIT License
 */

#include "OTAA.h"

OTAA::OTAA()
    : _state(OTA_IDLE)
    , _progress(0)
    , _checkInterval(6 * 60 * 60 * 1000)  // 6 hours
    , _lastCheckTime(0)
    , _autoCheck(false)
    , _initialized(false)
    , _stateCallback(nullptr)
    , _progressCallback(nullptr)
    , _errorCallback(nullptr) {
    _firmwareVersion = "1.0.0";
}

OTAA::~OTAA() {}

bool OTAA::begin(const char* serverUrl, const char* deviceId, const char* deviceToken) {
    _serverUrl = String(serverUrl);
    _deviceId = String(deviceId);
    _deviceToken = String(deviceToken);

    // Remove trailing slash
    if (_serverUrl.endsWith("/")) {
        _serverUrl.remove(_serverUrl.length() - 1);
    }

    _initialized = true;

    Serial.println("[OTAA] Initialized");
    Serial.println("[OTAA] Server: " + _serverUrl);
    Serial.println("[OTAA] Device ID: " + _deviceId);
    Serial.println("[OTAA] Firmware: " + _firmwareVersion);

    return true;
}

bool OTAA::beginWithActivationCode(const char* serverUrl, const char* activationCode) {
    _serverUrl = String(serverUrl);
    _activationCode = String(activationCode);

    // Remove trailing slash
    if (_serverUrl.endsWith("/")) {
        _serverUrl.remove(_serverUrl.length() - 1);
    }

    // Generate device ID
    _deviceId = generateDeviceId();

    Serial.println("[OTAA] Initializing with activation code");
    Serial.println("[OTAA] Server: " + _serverUrl);
    Serial.println("[OTAA] Device ID: " + _deviceId);

    // Register device
    if (!registerDevice()) {
        setError("Failed to register device");
        return false;
    }

    _initialized = true;
    return true;
}

void OTAA::setFirmwareVersion(const char* version) {
    _firmwareVersion = String(version);
}

void OTAA::setCheckInterval(unsigned long intervalMs) {
    _checkInterval = intervalMs;
}

void OTAA::setAutoCheck(bool enable) {
    _autoCheck = enable;
}

bool OTAA::autoCheck() {
    if (!_autoCheck || !_initialized) {
        return false;
    }

    unsigned long now = millis();
    if (now - _lastCheckTime >= _checkInterval) {
        _lastCheckTime = now;

        if (checkUpdate()) {
            return true;
        }
    }
    return false;
}

bool OTAA::checkUpdate() {
    if (!_initialized) {
        setError("Not initialized");
        return false;
    }

    setState(OTA_CHECKING);

    String url = _serverUrl + "/api/device/ota/check?current_version=" + _firmwareVersion;

    String response = httpGet(url);

    if (response.isEmpty()) {
        setState(OTA_FAILED);
        return false;
    }

    // Parse JSON
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, response);

    if (error) {
        setError("JSON parse error: " + String(error.c_str()));
        setState(OTA_FAILED);
        return false;
    }

    _firmwareInfo.updateAvailable = doc["update_available"].as<bool>();

    if (_firmwareInfo.updateAvailable) {
        _firmwareInfo.version = doc["version"].as<String>();
        _firmwareInfo.downloadUrl = doc["download_url"].as<String>();
        _firmwareInfo.fileSize = doc["file_size"].as<size_t>();
        _firmwareInfo.md5 = doc["file_md5"].as<String>();
        _firmwareInfo.releaseNotes = doc["release_notes"].as<String>();

        Serial.println("[OTAA] Update available: v" + _firmwareInfo.version);
        Serial.println("[OTAA] Current: v" + _firmwareVersion);

        setState(OTA_UPDATE_AVAILABLE);
        return true;
    } else {
        Serial.println("[OTAA] Firmware is up to date (v" + _firmwareVersion + ")");
        setState(OTA_IDLE);
        return false;
    }
}

bool OTAA::update() {
    if (!_firmwareInfo.updateAvailable) {
        setError("No update available");
        return false;
    }

    Serial.println("[OTAA] Starting update to v" + _firmwareInfo.version);
    setState(OTA_DOWNLOADING);

    if (!downloadFirmware()) {
        setState(OTA_FAILED);
        return false;
    }

    setState(OTA_SUCCESS);
    Serial.println("[OTAA] Update successful! Restarting...");

    delay(1000);

    #if defined(ESP32)
    ESP.restart();
    #elif defined(ESP8266)
    ESP.restart();
    #endif

    return true;
}

bool OTAA::registerDevice() {
    Serial.println("[OTAA] Registering device...");

    String url = _serverUrl + "/api/device/register";
    String json = "{";
    json += "\"device_id\":\"" + _deviceId + "\",";
    json += "\"activation_code\":\"" + _activationCode + "\",";
    json += "\"chip_model\":\"" + getChipId() + "\"";
    json += "}";

    String response = httpPost(url, json);

    if (response.isEmpty()) {
        return false;
    }

    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, response);

    if (error) {
        setError("JSON parse error");
        return false;
    }

    if (doc["success"].as<bool>()) {
        _deviceToken = doc["data"]["device_token"].as<String>();
        Serial.println("[OTAA] Device registered successfully");
        Serial.println("[OTAA] Token: " + _deviceToken.substring(0, 20) + "...");
        return true;
    } else {
        setError(doc["message"].as<String>());
        return false;
    }
}

OTAState OTAA::getState() {
    return _state;
}

int OTAA::getProgress() {
    return _progress;
}

String OTAA::getLastError() {
    return _lastError;
}

FirmwareInfo OTAA::getFirmwareInfo() {
    return _firmwareInfo;
}

String OTAA::getDeviceId() {
    return _deviceId;
}

String OTAA::getDeviceToken() {
    return _deviceToken;
}

void OTAA::onStateChange(OTAStateCallback callback) {
    _stateCallback = callback;
}

void OTAA::onProgress(OTAProgressCallback callback) {
    _progressCallback = callback;
}

void OTAA::onError(OTAErrorCallback callback) {
    _errorCallback = callback;
}

void OTAA::setState(OTAState state) {
    _state = state;
    if (_stateCallback) {
        _stateCallback(state);
    }
}

void OTAA::setProgress(int progress, size_t downloaded, size_t total) {
    _progress = progress;
    if (_progressCallback) {
        _progressCallback(progress, downloaded, total);
    }
}

void OTAA::setError(const String& error) {
    _lastError = error;
    Serial.println("[OTAA] Error: " + error);
    if (_errorCallback) {
        _errorCallback(error);
    }
}

bool OTAA::downloadFirmware() {
    Serial.println("[OTAA] Downloading firmware...");
    Serial.println("[OTAA] URL: " + _firmwareInfo.downloadUrl);
    Serial.println("[OTAA] Size: " + String(_firmwareInfo.fileSize) + " bytes");

    #if defined(ESP32)
    HTTPClient http;
    http.begin(_firmwareInfo.downloadUrl);
    http.addHeader("Authorization", "Bearer " + _deviceToken);

    int httpCode = http.GET();

    if (httpCode != 200) {
        setError("Download failed: HTTP " + String(httpCode));
        http.end();
        return false;
    }

    int totalSize = http.getSize();
    if (totalSize <= 0) {
        totalSize = _firmwareInfo.fileSize;
    }

    uint8_t* buffer = (uint8_t*)malloc(totalSize);
    if (!buffer) {
        setError("Memory allocation failed");
        http.end();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    size_t downloaded = 0;

    while (downloaded < totalSize) {
        size_t available = stream->available();
        if (available) {
            size_t bytesRead = stream->readBytes(buffer + downloaded, available);
            downloaded += bytesRead;

            int progress = (downloaded * 100) / totalSize;
            setProgress(progress, downloaded, totalSize);
        }
        delay(1);
        yield();
    }

    http.end();

    setState(OTA_INSTALLING);
    bool result = installFirmware(buffer, totalSize);
    free(buffer);

    return result;

    #elif defined(ESP8266)
    WiFiClient client;
    HTTPClient http;
    http.begin(client, _firmwareInfo.downloadUrl);
    http.addHeader("Authorization", "Bearer " + _deviceToken);

    int httpCode = http.GET();

    if (httpCode != 200) {
        setError("Download failed: HTTP " + String(httpCode));
        http.end();
        return false;
    }

    int totalSize = http.getSize();
    if (totalSize <= 0) {
        totalSize = _firmwareInfo.fileSize;
    }

    uint8_t* buffer = (uint8_t*)malloc(totalSize);
    if (!buffer) {
        setError("Memory allocation failed");
        http.end();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    size_t downloaded = 0;

    while (downloaded < totalSize) {
        size_t available = stream->available();
        if (available) {
            size_t bytesRead = stream->readBytes(buffer + downloaded, available);
            downloaded += bytesRead;

            int progress = (downloaded * 100) / totalSize;
            setProgress(progress, downloaded, totalSize);
        }
        delay(1);
        yield();
    }

    http.end();

    setState(OTA_INSTALLING);
    bool result = installFirmware(buffer, totalSize);
    free(buffer);

    return result;
    #endif
}

bool OTAA::installFirmware(uint8_t* data, size_t len) {
    Serial.println("[OTAA] Installing firmware...");

    #if defined(ESP32)
    if (!Update.begin(len)) {
        setError("Update begin failed");
        return false;
    }

    if (_firmwareInfo.md5.length() > 0) {
        Update.setMD5(_firmwareInfo.md5.c_str());
    }

    size_t written = Update.write(data, len);

    if (written != len) {
        setError("Write failed: " + String(written) + "/" + String(len));
        return false;
    }

    if (!Update.end(true)) {
        setError("Update end failed: " + String.getError());
        return false;
    }

    Serial.println("[OTAA] Firmware installed successfully");
    return true;

    #elif defined(ESP8266)
    if (!Update.begin(len)) {
        setError("Update begin failed");
        return false;
    }

    if (_firmwareInfo.md5.length() > 0) {
        Update.setMD5(_firmwareInfo.md5.c_str());
    }

    size_t written = Update.write(data, len);

    if (written != len) {
        setError("Write failed");
        return false;
    }

    if (!Update.end(true)) {
        setError("Update end failed");
        return false;
    }

    Serial.println("[OTAA] Firmware installed successfully");
    return true;
    #endif
}

String OTAA::httpGet(const String& url) {
    #if defined(ESP32)
    HTTPClient http;
    http.begin(url);
    http.addHeader("Authorization", "Bearer " + _deviceToken);
    http.setTimeout(30000);

    int httpCode = http.GET();

    if (httpCode != 200) {
        setError("HTTP GET failed: " + String(httpCode));
        http.end();
        return "";
    }

    String response = http.getString();
    http.end();
    return response;

    #elif defined(ESP8266)
    WiFiClient client;
    HTTPClient http;
    http.begin(client, url);
    http.addHeader("Authorization", "Bearer " + _deviceToken);
    http.setTimeout(30000);

    int httpCode = http.GET();

    if (httpCode != 200) {
        setError("HTTP GET failed: " + String(httpCode));
        http.end();
        return "";
    }

    String response = http.getString();
    http.end();
    return response;
    #endif
}

String OTAA::httpPost(const String& url, const String& json) {
    #if defined(ESP32)
    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + _deviceToken);
    http.setTimeout(30000);

    int httpCode = http.POST(json);

    if (httpCode != 200) {
        setError("HTTP POST failed: " + String(httpCode));
        http.end();
        return "";
    }

    String response = http.getString();
    http.end();
    return response;

    #elif defined(ESP8266)
    WiFiClient client;
    HTTPClient http;
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + _deviceToken);
    http.setTimeout(30000);

    int httpCode = http.POST(json);

    if (httpCode != 200) {
        setError("HTTP POST failed: " + String(httpCode));
        http.end();
        return "";
    }

    String response = http.getString();
    http.end();
    return response;
    #endif
}

String OTAA::generateDeviceId() {
    String chipId = getChipId();
    return "esp32_" + chipId;
}

String OTAA::getChipId() {
    #if defined(ESP32)
    uint64_t mac = ESP.getEfuseMac();
    char buf[13];
    sprintf(buf, "%012llX", mac);
    return String(buf).substring(4).toLowerCase();
    #elif defined(ESP8266)
    uint32_t chipId = ESP.getChipId();
    char buf[9];
    sprintf(buf, "%08X", chipId);
    return String(buf).toLowerCase();
    #endif
}
