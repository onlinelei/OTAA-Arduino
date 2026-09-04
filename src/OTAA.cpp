/**
 * OTAA - OTA Update Library for Arduino
 * v1.3.0 - ArduinoJson v7, streaming download, A/B rollback, heartbeat, NVS
 *
 * MIT License
 */

#include "OTAA.h"

OTAA::OTAA()
    : _state(OTA_IDLE)
    , _progress(0)
    , _checkInterval(60 * 1000)  // 1 minute
    , _lastCheckTime(0)
    , _autoCheck(false)
    , _initialized(false)
    , _heartbeatInterval(60 * 1000)  // 60 seconds
    , _lastHeartbeatTime(0)
    , _forceUpdate(false)
    , _credentialSave(nullptr)
    , _credentialLoad(nullptr)
    , _currentCommandId(0)
    , _commandStartTime(0)
    , _commandCheckInterval(5000)  // 5 seconds
    , _commandTimeout(300000)      // 5 minutes
    , _lastCommandCheckTime(0)
    , _commandCallback(nullptr)
    , _logUploadInterval(20000)    // 20 seconds
    , _lastLogUploadTime(0)
    , _timeSynced(false)
    , _stateCallback(nullptr)
    , _progressCallback(nullptr)
    , _errorCallback(nullptr) {
    _firmwareVersion = "1.0.0";
}

OTAA::~OTAA() {}

// ========== 固件确认 (ESP32 A/B 回滚) ==========

void OTAA::confirmFirmwareValid() {
#if defined(ESP32)
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(running, &state) == ESP_OK
            && state == ESP_OTA_IMG_PENDING_VERIFY) {
        esp_ota_mark_app_valid_cancel_rollback();
        Serial.println("[OTAA] Firmware confirmed valid (rollback cancelled)");
    }
#endif
}

// ========== 凭证存储 ==========

void OTAA::saveCredentials() {
    if (_credentialSave) {
        _credentialSave(_deviceId, _deviceToken);
        Serial.println("[OTAA] Credentials saved to storage");
    }
}

bool OTAA::loadCredentials() {
    if (_credentialLoad) {
        String loadedId, loadedToken;
        if (_credentialLoad(loadedId, loadedToken)) {
            _deviceId = loadedId;
            _deviceToken = loadedToken;
            Serial.println("[OTAA] Credentials loaded from storage");
            return true;
        }
    }
    return false;
}

// ========== 初始化 ==========

bool OTAA::begin(const char* serverUrl, const char* deviceId, const char* deviceToken) {
    confirmFirmwareValid();

    _serverUrl = String(serverUrl);
    _deviceId = String(deviceId);
    _deviceToken = String(deviceToken);

    if (_serverUrl.endsWith("/")) {
        _serverUrl.remove(_serverUrl.length() - 1);
    }

    _initialized = true;

    Serial.println("[OTAA] Initialized");
    Serial.println("[OTAA] Server: " + _serverUrl);
    Serial.println("[OTAA] Device ID: " + _deviceId);
    Serial.println("[OTAA] Firmware: " + _firmwareVersion);
    
    otaLog("[OTAA] Initialized, deviceId: " + _deviceId);

    return true;
}

bool OTAA::beginWithActivationCode(const char* serverUrl, const char* activationCode) {
    confirmFirmwareValid();

    _serverUrl = String(serverUrl);
    _activationCode = String(activationCode);

    if (_serverUrl.endsWith("/")) {
        _serverUrl.remove(_serverUrl.length() - 1);
    }

    _deviceId = generateDeviceId();

    Serial.println("[OTAA] Initializing with activation code");
    Serial.println("[OTAA] Server: " + _serverUrl);
    Serial.println("[OTAA] Device ID: " + _deviceId);

    // 优先从持久化存储加载凭证，避免重复注册
    if (loadCredentials()) {
        Serial.println("[OTAA] Loaded saved credentials, skipping registration");
        _initialized = true;
        return true;
    }

    // 注册设备
    if (!registerDevice()) {
        setError("Failed to register device");
        return false;
    }

    // 持久化凭证
    saveCredentials();

    _initialized = true;
    return true;
}

// ========== 设置方法 ==========

void OTAA::setFirmwareVersion(const char* version) { _firmwareVersion = String(version); }
void OTAA::setCheckInterval(unsigned long intervalMs) { _checkInterval = intervalMs; }
void OTAA::setCommandCheckInterval(unsigned long intervalMs) { _commandCheckInterval = intervalMs; }
void OTAA::setCommandTimeout(unsigned long timeoutMs) { _commandTimeout = timeoutMs; }
void OTAA::setAutoCheck(bool enable) { _autoCheck = enable; }
void OTAA::setHeartbeatInterval(unsigned long intervalMs) { _heartbeatInterval = intervalMs; }
void OTAA::setCredentialStorage(CredentialSaveCallback save, CredentialLoadCallback load) {
    _credentialSave = save;
    _credentialLoad = load;
}

// ========== 心跳 ==========

bool OTAA::heartbeat() {
    if (!_initialized || _deviceToken.length() == 0) return false;

    String url = _serverUrl + "/api/device/heartbeat";

    DynamicJsonDocument doc(256);
    doc["fwVersion"] = _firmwareVersion;
    doc["ipAddress"] = WiFi.localIP().toString();

    String json;
    serializeJson(doc, json);

    String response = httpPost(url, json);
    if (response.isEmpty()) return false;

    DynamicJsonDocument respDoc(256);
    DeserializationError error = deserializeJson(respDoc, response);
    if (error) return false;

    if (respDoc["success"].as<bool>()) {
        JsonObject data = respDoc["data"];
        _forceUpdate = data["forceUpdate"].as<bool>();
        if (_forceUpdate) {
            Serial.println("[OTAA] Server requests force update!");
            _lastCheckTime = 0;  // 立即触发检查
        }
        return true;
    }
    return false;
}

// ========== 自动检查 ==========

bool OTAA::autoCheck() {
    if (!_autoCheck || !_initialized) return false;

    unsigned long now = millis();

    // 心跳（高频，默认 60s）
    if (now - _lastHeartbeatTime >= _heartbeatInterval) {
        _lastHeartbeatTime = now;
        heartbeat();
    }

    // 检查固件更新（低频，默认 6h，或 forceUpdate 触发）
    if (_forceUpdate || now - _lastCheckTime >= _checkInterval) {
        _lastCheckTime = now;
        _forceUpdate = false;
        
        // 检测到更新后自动执行升级
        if (checkUpdate()) {
            Serial.println("[OTAA] Update detected, starting auto update...");
            otaLog("[OTAA] Update detected, starting auto update...");
            update();
        }
    }

    // 检查命令（高频，默认 5s）
    if (now - _lastCommandCheckTime >= _commandCheckInterval) {
        _lastCommandCheckTime = now;
        checkCommands();
    }

    // 检查当前命令是否超时
    if (_currentCommandId > 0 && (now - _commandStartTime > _commandTimeout)) {
        Serial.println("[OTAA] Command timeout: " + String(_currentCommandId));
        ackCommand(_currentCommandId, false, "", "Timeout");
        _currentCommandId = 0;
    }

    // 日志上报（定时或缓冲区满）
    if (now - _lastLogUploadTime >= _logUploadInterval || isLogBufferFull()) {
        _lastLogUploadTime = now;
        uploadLogs();
    }

    return false;
}

// ========== 检查更新 ==========

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

    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, response);

    if (error) {
        setError("JSON parse error: " + String(error.c_str()));
        setState(OTA_FAILED);
        return false;
    }

    if (!doc["success"].as<bool>()) {
        setError(doc["message"].as<String>());
        setState(OTA_FAILED);
        return false;
    }

    JsonObject data = doc["data"];
    _firmwareInfo.updateAvailable = data["updateAvailable"].as<bool>();

    if (_firmwareInfo.updateAvailable) {
        _firmwareInfo.version = data["version"].as<String>();
        _firmwareInfo.downloadUrl = data["downloadUrl"].as<String>();
        _firmwareInfo.fileSize = data["fileSize"].as<size_t>();
        _firmwareInfo.md5 = data["fileMd5"].as<String>();
        _firmwareInfo.releaseNotes = data["releaseNotes"].as<String>();

        Serial.println("[OTAA] Update available: v" + _firmwareInfo.version);
        Serial.println("[OTAA] Current: v" + _firmwareVersion);
        otaLog("[OTAA] Update available: v" + _firmwareInfo.version + ", current: v" + _firmwareVersion);

        setState(OTA_UPDATE_AVAILABLE);
        return true;
    } else {
        Serial.println("[OTAA] Firmware is up to date (v" + _firmwareVersion + ")");
        setState(OTA_IDLE);
        return false;
    }
}

// ========== 执行更新 ==========

bool OTAA::update() {
    if (!_firmwareInfo.updateAvailable) {
        setError("No update available");
        return false;
    }

    String fromVersion = _firmwareVersion;
    String toVersion = _firmwareInfo.version;

    Serial.println("[OTAA] Starting update to v" + toVersion);
    otaLog("[OTAA] Starting update from v" + fromVersion + " to v" + toVersion);
    setState(OTA_DOWNLOADING);

    // 上报下载开始状态
    reportOtaResult(fromVersion, toVersion, "downloading");

    if (!downloadFirmware()) {
        setState(OTA_FAILED);
        // 上报失败状态
        reportOtaResult(fromVersion, toVersion, "failed", _lastError);
        return false;
    }

    setState(OTA_SUCCESS);
    Serial.println("[OTAA] Update successful! Restarting...");
    otaLog("[OTAA] Update successful from v" + fromVersion + " to v" + toVersion);

    // 上报成功状态
    reportOtaResult(fromVersion, toVersion, "success");

    delay(1000);
    ESP.restart();
    return true;
}

// ========== 设备注册 ==========

bool OTAA::registerDevice() {
    Serial.println("[OTAA] Registering device...");
    otaLog("[OTAA] Registering device...");

    String url = _serverUrl + "/api/device/register";
    String chipId = getChipId();

    DynamicJsonDocument doc(512);
    doc["activationCode"] = _activationCode;
    doc["chipModel"] = "esp32";
    doc["chipId"] = chipId;
    doc["fwVersion"] = _firmwareVersion;

    String json;
    serializeJson(doc, json);

    Serial.println("[OTAA] Request: " + json);

    String response = httpPost(url, json);
    if (response.isEmpty()) return false;

    DynamicJsonDocument respDoc(512);
    DeserializationError error = deserializeJson(respDoc, response);

    if (error) {
        setError("JSON parse error");
        return false;
    }

    if (respDoc["success"].as<bool>()) {
        _deviceId = respDoc["data"]["deviceId"].as<String>();
        _deviceToken = respDoc["data"]["deviceToken"].as<String>();
        Serial.println("[OTAA] Device registered successfully");
        Serial.println("[OTAA] Device ID: " + _deviceId);
        Serial.println("[OTAA] Token: " + _deviceToken.substring(0, 20) + "...");
        otaLog("[OTAA] Device registered successfully, deviceId: " + _deviceId);
        return true;
    } else {
        setError(respDoc["message"].as<String>());
        return false;
    }
}

// ========== 状态查询 ==========

OTAState OTAA::getState() { return _state; }
int OTAA::getProgress() { return _progress; }
String OTAA::getLastError() { return _lastError; }
FirmwareInfo OTAA::getFirmwareInfo() { return _firmwareInfo; }
String OTAA::getDeviceId() { return _deviceId; }
String OTAA::getDeviceToken() { return _deviceToken; }

// ========== 回调 ==========

void OTAA::onStateChange(OTAStateCallback callback) { _stateCallback = callback; }
void OTAA::onProgress(OTAProgressCallback callback) { _progressCallback = callback; }
void OTAA::onError(OTAErrorCallback callback) { _errorCallback = callback; }

void OTAA::setState(OTAState state) {
    _state = state;
    if (_stateCallback) _stateCallback(state);
}

void OTAA::setProgress(int progress, size_t downloaded, size_t total) {
    _progress = progress;
    if (_progressCallback) _progressCallback(progress, downloaded, total);
}

void OTAA::setError(const String& error) {
    _lastError = error;
    Serial.println("[OTAA] Error: " + error);
    if (_errorCallback) _errorCallback(error);
}

// ========== 流式固件下载 ==========

bool OTAA::downloadFirmware() {
    Serial.println("[OTAA] Downloading firmware (streaming)...");
    Serial.println("[OTAA] URL: " + _firmwareInfo.downloadUrl);
    Serial.println("[OTAA] Size: " + String(_firmwareInfo.fileSize) + " bytes");
    otaLog("[OTAA] Downloading firmware v" + _firmwareInfo.version + ", size: " + String(_firmwareInfo.fileSize) + " bytes");

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

    size_t totalSize = http.getSize();
    if (totalSize <= 0) totalSize = _firmwareInfo.fileSize;

    if (!Update.begin(totalSize)) {
        setError("Update begin failed");
        http.end();
        return false;
    }

    if (_firmwareInfo.md5.length() > 0) {
        Update.setMD5(_firmwareInfo.md5.c_str());
    }

    uint8_t buffer[4096];
    WiFiClient* stream = http.getStreamPtr();
    size_t downloaded = 0;

    while (downloaded < totalSize) {
        size_t available = stream->available();
        if (available) {
            size_t toRead = (available > sizeof(buffer)) ? sizeof(buffer) : available;
            size_t bytesRead = stream->readBytes(buffer, toRead);
            size_t written = Update.write(buffer, bytesRead);
            if (written != bytesRead) {
                setError("Write failed: " + String(written) + "/" + String(bytesRead));
                http.end();
                return false;
            }
            downloaded += bytesRead;
            setProgress((downloaded * 100) / totalSize, downloaded, totalSize);
        }
        delay(1);
        yield();
    }

    http.end();

    setState(OTA_INSTALLING);
    if (!Update.end(true)) {
        setError("Update end failed: " + String(Update.errorString()));
        return false;
    }

    Serial.println("[OTAA] Firmware installed successfully");
    otaLog("[OTAA] Firmware installed successfully");
    return true;

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

    size_t totalSize = http.getSize();
    if (totalSize <= 0) totalSize = _firmwareInfo.fileSize;

    if (!Update.begin(totalSize)) {
        setError("Update begin failed");
        http.end();
        return false;
    }

    if (_firmwareInfo.md5.length() > 0) {
        Update.setMD5(_firmwareInfo.md5.c_str());
    }

    uint8_t buffer[4096];
    WiFiClient* stream = http.getStreamPtr();
    size_t downloaded = 0;

    while (downloaded < totalSize) {
        size_t available = stream->available();
        if (available) {
            size_t toRead = (available > sizeof(buffer)) ? sizeof(buffer) : available;
            size_t bytesRead = stream->readBytes(buffer, toRead);
            size_t written = Update.write(buffer, bytesRead);
            if (written != bytesRead) {
                setError("Write failed");
                http.end();
                return false;
            }
            downloaded += bytesRead;
            setProgress((downloaded * 100) / totalSize, downloaded, totalSize);
        }
        delay(1);
        yield();
    }

    http.end();

    setState(OTA_INSTALLING);
    if (!Update.end(true)) {
        setError("Update end failed");
        return false;
    }

    Serial.println("[OTAA] Firmware installed successfully");
    otaLog("[OTAA] Firmware installed successfully");
    return true;
#endif
}

// ========== HTTP 工具 ==========

String OTAA::httpGet(const String& url) {
#if defined(ESP32)
    HTTPClient http;
    http.begin(url);
    if (_deviceToken.length() > 0) {
        http.addHeader("Authorization", "Bearer " + _deviceToken);
    }
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
    if (_deviceToken.length() > 0) {
        http.addHeader("Authorization", "Bearer " + _deviceToken);
    }
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
    if (_deviceToken.length() > 0) {
        http.addHeader("Authorization", "Bearer " + _deviceToken);
    }
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
    if (_deviceToken.length() > 0) {
        http.addHeader("Authorization", "Bearer " + _deviceToken);
    }
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

String OTAA::httpPostMultipart(const String& url, const String& fieldName,
                                const uint8_t* data, size_t len, const String& filename) {
#if defined(ESP32)
    HTTPClient http;
    http.begin(url);
    if (_deviceToken.length() > 0) {
        http.addHeader("Authorization", "Bearer " + _deviceToken);
    }

    String boundary = "----OTAA" + String(millis());
    String contentType = "multipart/form-data; boundary=" + boundary;
    http.addHeader("Content-Type", contentType);

    String bodyStart = "--" + boundary + "\r\n";
    bodyStart += "Content-Disposition: form-data; name=\"" + fieldName + "\"; filename=\"" + filename + "\"\r\n";
    bodyStart += "Content-Type: application/octet-stream\r\n\r\n";
    String bodyEnd = "\r\n--" + boundary + "--\r\n";

    size_t totalLen = bodyStart.length() + len + bodyEnd.length();
    uint8_t* postData = (uint8_t*)malloc(totalLen);
    if (!postData) {
        setError("Memory allocation failed");
        http.end();
        return "";
    }

    memcpy(postData, bodyStart.c_str(), bodyStart.length());
    memcpy(postData + bodyStart.length(), data, len);
    memcpy(postData + bodyStart.length() + len, bodyEnd.c_str(), bodyEnd.length());

    int httpCode = http.POST(postData, totalLen);
    free(postData);

    if (httpCode != 200) {
        setError("Upload failed: HTTP " + String(httpCode));
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
    if (_deviceToken.length() > 0) {
        http.addHeader("Authorization", "Bearer " + _deviceToken);
    }

    String boundary = "----OTAA" + String(millis());
    String contentType = "multipart/form-data; boundary=" + boundary;
    http.addHeader("Content-Type", contentType);

    String bodyStart = "--" + boundary + "\r\n";
    bodyStart += "Content-Disposition: form-data; name=\"" + fieldName + "\"; filename=\"" + filename + "\"\r\n";
    bodyStart += "Content-Type: application/octet-stream\r\n\r\n";
    String bodyEnd = "\r\n--" + boundary + "--\r\n";

    size_t totalLen = bodyStart.length() + len + bodyEnd.length();
    uint8_t* postData = (uint8_t*)malloc(totalLen);
    if (!postData) {
        setError("Memory allocation failed");
        http.end();
        return "";
    }

    memcpy(postData, bodyStart.c_str(), bodyStart.length());
    memcpy(postData + bodyStart.length(), data, len);
    memcpy(postData + bodyStart.length() + len, bodyEnd.c_str(), bodyEnd.length());

    int httpCode = http.POST(postData, totalLen);
    free(postData);

    if (httpCode != 200) {
        setError("Upload failed: HTTP " + String(httpCode));
        http.end();
        return "";
    }

    String response = http.getString();
    http.end();
    return response;
#endif
}

// ========== 设备 ID ==========

String OTAA::generateDeviceId() {
    String chipId = getChipId();
    byte shaResult[32];

#if defined(ESP32)
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, (const unsigned char*)chipId.c_str(), chipId.length());
    mbedtls_md_finish(&ctx, shaResult);
    mbedtls_md_free(&ctx);
#elif defined(ESP8266)
    SHA256 sha;
    sha.reset();
    sha.update(chipId.c_str(), chipId.length());
    sha.finalize(shaResult, 32);
#endif

    char hash[17];
    sprintf(hash, "%02x%02x%02x%02x%02x%02x%02x%02x",
            shaResult[0], shaResult[1], shaResult[2], shaResult[3],
            shaResult[4], shaResult[5], shaResult[6], shaResult[7]);

    return "esp32_" + String(hash);
}

String OTAA::getChipId() {
#if defined(ESP32)
    uint64_t mac = ESP.getEfuseMac();
    char buf[13];
    sprintf(buf, "%012llX", mac);
    String chipId = String(buf).substring(4);
    chipId.toLowerCase();
    return chipId;
#elif defined(ESP8266)
    uint32_t chipId = ESP.getChipId();
    char buf[9];
    sprintf(buf, "%08X", chipId);
    String id = String(buf);
    id.toLowerCase();
    return id;
#endif
}

// ========== 命令 ==========

void OTAA::onCommand(CommandCallback callback) { _commandCallback = callback; }

bool OTAA::checkCommands() {
    if (!_initialized) return false;
    if (_currentCommandId > 0) return false;

    DeviceCommand cmd = fetchPendingCommand();
    if (cmd.id > 0) {
        _currentCommandId = cmd.id;
        _commandStartTime = millis();

        Serial.println("[OTAA] Received command: " + cmd.command + " (ID: " + String(cmd.id) + ")");

        if (_commandCallback) {
            _commandCallback(cmd.id, cmd.command, cmd.params);
        }
        return true;
    }
    return false;
}

DeviceCommand OTAA::fetchPendingCommand() {
    DeviceCommand cmd;
    cmd.id = 0;

    String url = _serverUrl + "/api/device/commands/pending";
    String response = httpGet(url);

    if (response.isEmpty()) return cmd;

    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, response);
    if (error) return cmd;

    if (!doc["success"].as<bool>()) return cmd;
    if (doc["data"].isNull()) return cmd;

    JsonObject data = doc["data"];
    cmd.id = data["id"].as<long>();
    cmd.command = data["command"].as<String>();
    cmd.params = data["params"].as<String>();

    return cmd;
}

bool OTAA::ackCommand(int commandId, bool success, const String& result, const String& errorMsg) {
    String url = _serverUrl + "/api/device/commands/" + String(commandId) + "/ack";

    DynamicJsonDocument doc(512);
    doc["status"] = success ? 2 : 3;
    if (result.length() > 0) doc["result"] = result;
    if (errorMsg.length() > 0) doc["errorMsg"] = errorMsg;

    String json;
    serializeJson(doc, json);

    String response = httpPost(url, json);
    bool ok = !response.isEmpty();

    if (commandId == _currentCommandId) {
        _currentCommandId = 0;
    }

    return ok;
}

bool OTAA::uploadFile(int commandId, const String& fieldName, const uint8_t* data, size_t len, const String& filename) {
    String url = _serverUrl + "/api/device/commands/" + String(commandId) + "/upload";
    return httpPostMultipart(url, fieldName, data, len, filename);
}

// ========== OTA 结果上报 ==========

bool OTAA::reportOtaResult(const String& fromVersion, const String& toVersion, 
                           const String& status, const String& errorMsg) {
    if (!_initialized || _deviceToken.length() == 0) {
        Serial.println("[OTAA] Cannot report OTA result: not initialized or no token");
        return false;
    }
    
    String url = _serverUrl + "/api/device/ota/report";
    
    DynamicJsonDocument doc(256);
    doc["fromVersion"] = fromVersion;
    doc["toVersion"] = toVersion;
    doc["status"] = status;
    if (errorMsg.length() > 0) {
        doc["errorMsg"] = errorMsg;
    }
    
    String json;
    serializeJson(doc, json);
    
    Serial.println("[OTAA] Reporting OTA result: " + status);
    Serial.println("[OTAA] From: v" + fromVersion + " -> To: v" + toVersion);
    
    String response = httpPost(url, json);
    if (response.isEmpty()) {
        Serial.println("[OTAA] Failed to report OTA result");
        return false;
    }
    
    DynamicJsonDocument respDoc(256);
    DeserializationError error = deserializeJson(respDoc, response);
    if (error) {
        Serial.println("[OTAA] Failed to parse OTA report response");
        return false;
    }
    
    if (respDoc["success"].as<bool>()) {
        Serial.println("[OTAA] OTA result reported successfully");
        return true;
    } else {
        Serial.println("[OTAA] OTA report failed: " + respDoc["message"].as<String>());
        return false;
    }
}

// ========== 日志上报相关 ==========

void OTAA::setLogUploadInterval(unsigned long intervalMs) {
    _logUploadInterval = intervalMs;
}

bool OTAA::uploadLogs() {
    if (!_initialized || _deviceToken.length() == 0) {
        Serial.println("[OTAA] Cannot upload logs: not initialized or no token");
        return false;
    }
    
    // 获取日志缓冲区中的日志
    std::vector<String> logs = OTALogger::getInstance().getLogs();
    if (logs.empty()) {
        return true;
    }
    
    // 构建上传数据
    DynamicJsonDocument doc(4096);
    JsonArray logsArray = doc.createNestedArray("logs");
    for (const String& log : logs) {
        logsArray.add(log);
    }
    
    String json;
    serializeJson(doc, json);
    
    // 上报日志
    String url = _serverUrl + "/api/device/logs/upload";
    Serial.println("[OTAA] Uploading " + String(logs.size()) + " logs...");
    
    String response = httpPost(url, json);
    if (response.isEmpty()) {
        Serial.println("[OTAA] Failed to upload logs");
        return false;
    }
    
    DynamicJsonDocument respDoc(256);
    DeserializationError error = deserializeJson(respDoc, response);
    if (error) {
        Serial.println("[OTAA] Failed to parse log upload response");
        return false;
    }
    
    if (respDoc["success"].as<bool>()) {
        Serial.println("[OTAA] Logs uploaded successfully");
        otaLog("[OTAA] Logs uploaded successfully, count: " + String(logs.size()));
        OTALogger::getInstance().clearBuffer();
        _lastLogUploadTime = millis();
        return true;
    } else {
        Serial.println("[OTAA] Log upload failed: " + respDoc["message"].as<String>());
        return false;
    }
}

bool OTAA::isLogBufferFull() {
    return OTALogger::getInstance().getBufferSize() >= LOG_BUFFER_SIZE;
}

void OTAA::syncTime() {
    // 默认 NTP 服务器：使用国内可达的公共 NTP
    // pool.ntp.org / time.nist.gov 在国内常被运营商屏蔽 UDP 123，导致同步失败
    configTime(8 * 3600, 0, "ntp.aliyun.com", "cn.pool.ntp.org");
    // 显式设置时区环境变量，确保 localtime() 返回 UTC+8
    // POSIX TZ 格式：CST-8 表示 UTC+8（负偏移 = 东时区）
    setenv("TZ", "CST-8", 1);
    tzset();

    // 等待时间同步（最长 10s，20 次 × 500ms）
    int retry = 0;
    while (time(nullptr) < 1000000000 && retry < 20) {
        delay(500);
        retry++;
    }

    if (time(nullptr) >= 1000000000) {
        _timeSynced = true;
        time_t now = time(nullptr);
        struct tm* timeinfo = localtime(&now);
        char timeStr[32];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
        Serial.printf("[OTAA] NTP time synced: %s (UTC+8)\n", timeStr);
    } else {
        _timeSynced = false;
        _lastError = "NTP timeout (10s)";
        Serial.println("[OTAA] NTP time sync failed - check WiFi/firewall/NTP UDP:123");
    }
}
