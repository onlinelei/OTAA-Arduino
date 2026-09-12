/**
 * OTAA - Core implementation (platform-agnostic)
 *
 * All platform calls go through _hal->xxx() — no direct Arduino or ESP-IDF API usage.
 * This file compiles on any framework as long as OTAAHAL is implemented.
 */

#include "OTAA.h"
#include "CommandDispatcher.h"

// ESP-IDF 兼容：Arduino 的 byte 类型
#if !defined(ARDUINO)
typedef uint8_t byte;
#endif

// ========== 构造 / 析构 ==========

OTAA::OTAA()
    : _hal(nullptr)
    , _ownsHal(false)
    , _state(OTA_IDLE)
    , _progress(0)
    , _checkInterval(60 * 1000)
    , _lastCheckTime(0)
    , _autoCheck(false)
    , _initialized(false)
    , _heartbeatInterval(60 * 1000)
    , _lastHeartbeatTime(0)
    , _forceUpdate(false)
    , _credentialSave(nullptr)
    , _credentialLoad(nullptr)
    , _currentCommandId(0)
    , _commandStartTime(0)
    , _commandCheckInterval(5000)
    , _commandTimeout(300000)
    , _lastCommandCheckTime(0)
    , _commandCallback(nullptr)
    , _logUploadInterval(20000)
    , _lastLogUploadTime(0)
    , _timeSynced(false)
    , _stateCallback(nullptr)
    , _progressCallback(nullptr)
    , _errorCallback(nullptr)
{
    _firmwareVersion = "1.0.0";

    // 自动创建默认 HAL（堆分配，避免 static 局部变量的初始化顺序问题）
#if defined(ARDUINO)
    _hal = new ArduinoHal();
    _ownsHal = true;
#elif defined(ESP_IDF_VERSION)
    _hal = new EspHal();
    _ownsHal = true;
#endif
}

OTAA::OTAA(OTAAHAL* hal)
    : OTAA()  // 先调默认构造初始化所有字段
{
    _hal = hal;
    _ownsHal = false;
}

OTAA::~OTAA() {
    if (_ownsHal && _hal) {
        delete _hal;
        _hal = nullptr;
    }
}

// ========== 凭证存储 ==========

void OTAA::saveCredentials() {
    if (_credentialSave) {
        _credentialSave(_deviceId, _deviceToken);
        _hal->log('I', "OTAA", "Credentials saved to storage");
    }
}

bool OTAA::loadCredentials() {
    if (_credentialLoad) {
        String loadedId, loadedToken;
        if (_credentialLoad(loadedId, loadedToken)) {
            _deviceId = loadedId;
            _deviceToken = loadedToken;
            _hal->log('I', "OTAA", "Credentials loaded from storage");
            return true;
        }
    }
    return false;
}

// ========== 初始化 ==========

void OTAA::confirmFirmwareValid() {
    if (!_hal) return;
    _hal->confirmFirmwareValid();
}

void OTAA::computeFirmwareMD5() {
    _firmwareMd5 = _hal->getSketchMD5();
    if (_firmwareMd5.length() > 0) {
        _hal->log('I', "OTAA", "Firmware MD5: %s", _firmwareMd5.c_str());
    }
}

bool OTAA::begin(const char* serverUrl, const char* deviceId, const char* deviceToken) {
    if (!_hal) {
        _lastError = "HAL not initialized";
        return false;
    }
    confirmFirmwareValid();
    computeFirmwareMD5();

    _serverUrl = String(serverUrl);
    _deviceId = String(deviceId);
    _deviceToken = String(deviceToken);

    // 去掉末尾斜杠
    while (_serverUrl.length() > 0 && _serverUrl.endsWith("/")) {
        _serverUrl = _serverUrl.substring(0, _serverUrl.length() - 1);
    }

    _initialized = true;

    _hal->log('I', "OTAA", "Initialized, server: %s, deviceId: %s, fw: %s",
              _serverUrl.c_str(), _deviceId.c_str(), _firmwareVersion.c_str());

    otaLog(String("[OTAA] Initialized, deviceId: ") + _deviceId.c_str());

    return true;
}

bool OTAA::beginWithActivationCode(const char* serverUrl, const char* activationCode) {
    if (!_hal) {
        _lastError = "HAL not initialized";
        return false;
    }
    confirmFirmwareValid();
    computeFirmwareMD5();

    _serverUrl = String(serverUrl);
    _activationCode = String(activationCode);

    while (_serverUrl.length() > 0 && _serverUrl.endsWith("/")) {
        _serverUrl = _serverUrl.substring(0, _serverUrl.length() - 1);
    }

    _deviceId = generateDeviceId();

    _hal->log('I', "OTAA", "Init with activation code, server: %s, deviceId: %s",
              _serverUrl.c_str(), _deviceId.c_str());

    if (loadCredentials()) {
        _hal->log('I', "OTAA", "Loaded saved credentials, skipping registration");
        _initialized = true;
        return true;
    }

    if (!registerDevice()) {
        setError("Failed to register device");
        return false;
    }

    saveCredentials();
    _initialized = true;
    return true;
}

// ========== 配置 ==========

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
void OTAA::setLogUploadInterval(unsigned long intervalMs) { _logUploadInterval = intervalMs; }

// ========== 心跳 ==========

bool OTAA::heartbeat() {
    if (!_initialized || _deviceToken.length() == 0) return false;

    String url = _serverUrl + "/api/device/heartbeat";

    DynamicJsonDocument doc(256);
    doc["fwVersion"] = _firmwareVersion;
    doc["ipAddress"] = _hal->getLocalIP();

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
            _hal->log('W', "OTAA", "Server requests force update!");
            _lastCheckTime = 0;
        }
        return true;
    }
    return false;
}

// ========== 自动检查 ==========

bool OTAA::autoCheck() {
    if (!_autoCheck || !_initialized) return false;

    unsigned long now = _hal->millis();

    // 心跳
    if (now - _lastHeartbeatTime >= _heartbeatInterval) {
        _lastHeartbeatTime = now;
        heartbeat();
    }

    // 固件更新检查
    if (_forceUpdate || now - _lastCheckTime >= _checkInterval) {
        _lastCheckTime = now;
        _forceUpdate = false;

        if (checkUpdate()) {
            _hal->log('I', "OTAA", "Update detected, starting auto update...");
            otaLog("[OTAA] Update detected, starting auto update...");
            update();
        }
    }

    // 命令检查
    if (now - _lastCommandCheckTime >= _commandCheckInterval) {
        _lastCommandCheckTime = now;
        checkCommands();
    }

    // 命令超时
    // ⚠️ 必须在这里重新取一次 millis()：本轮的 `now` 是函数入口取的，
    // 而 checkCommands() 里的 _commandStartTime 是在 fetchPendingCommand()
    // 的 HTTP 请求返回之后才赋值的，必然 >= now。拿 now 去减会让
    // unsigned long 下溢成 ~42 亿，于是命令刚收到的同一轮就误判超时
    // （实测 play_audio 收到命令 3ms 后即报 "Command timeout: 36"，
    //   失败的 ack 先落地，平台随后拒绝播放成功的 ack）。
    if (_currentCommandId > 0 &&
        (_hal->millis() - _commandStartTime > _commandTimeout)) {
        _hal->log('W', "OTAA", "Command timeout: %d", _currentCommandId);
        ackCommand(_currentCommandId, false, "", "Timeout");
        _currentCommandId = 0;
    }

    // 日志上报
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
    if (_firmwareMd5.length() > 0) {
        url += "&firmware_md5=" + _firmwareMd5;
    }
    String response = httpGet(url);

    if (response.isEmpty()) {
        setState(OTA_FAILED);
        return false;
    }

    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, response);

    if (error) {
        setError(String("JSON parse error: ") + error.c_str());
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

        _hal->log('I', "OTAA", "Update available: v%s, current: v%s",
                  _firmwareInfo.version.c_str(), _firmwareVersion.c_str());
        otaLog(String("[OTAA] Update available: v") + _firmwareInfo.version.c_str()
               + ", current: v" + _firmwareVersion.c_str());

        setState(OTA_UPDATE_AVAILABLE);
        return true;
    } else {
        _hal->log('I', "OTAA", "Firmware is up to date (v%s)", _firmwareVersion.c_str());
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

    _hal->log('I', "OTAA", "Starting update to v%s", toVersion.c_str());
    otaLog(String("[OTAA] Starting update from v") + fromVersion.c_str() + " to v" + toVersion.c_str());
    setState(OTA_DOWNLOADING);

    reportOtaResult(fromVersion, toVersion, "downloading");

    if (!downloadFirmware()) {
        setState(OTA_FAILED);
        reportOtaResult(fromVersion, toVersion, "failed", _lastError);
        return false;
    }

    setState(OTA_SUCCESS);
    _hal->log('I', "OTAA", "Update successful! Restarting...");
    otaLog(String("[OTAA] Update successful from v") + fromVersion.c_str() + " to v" + toVersion.c_str());

    reportOtaResult(fromVersion, toVersion, "success");

    _hal->delay(1000);
    _hal->restart();
    return true;
}

// ========== 设备注册 ==========

bool OTAA::registerDevice() {
    _hal->log('I', "OTAA", "Registering device...");
    otaLog("[OTAA] Registering device...");

    String url = _serverUrl + "/api/device/register";
    String chipId = _hal->getChipId();

    DynamicJsonDocument doc(512);
    doc["activationCode"] = _activationCode;
    doc["chipModel"] = "esp32";
    doc["chipId"] = chipId;
    doc["fwVersion"] = _firmwareVersion;

    String json;
    serializeJson(doc, json);

    _hal->log('I', "OTAA", "Register request: %s", json.c_str());

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
        _hal->log('I', "OTAA", "Device registered: %s", _deviceId.c_str());
        otaLog(String("[OTAA] Device registered, deviceId: ") + _deviceId.c_str());
        return true;
    } else {
        setError(respDoc["message"].as<String>());
        return false;
    }
}

// ========== 流式固件下载 ==========

// OTA 流式回调上下文
struct OtaStreamCtx {
    OTAA* self;
    size_t totalSize;
    size_t downloaded;
    bool failed;
};

bool otaStreamCallback(const uint8_t* data, size_t len, void* userdata) {
    OtaStreamCtx* ctx = (OtaStreamCtx*)userdata;
    OTAA* self = ctx->self;

    size_t written = self->getHal()->otaWrite(data, len);
    if (written == 0) {
        ctx->failed = true;
        return false;
    }

    ctx->downloaded += len;
    if (ctx->totalSize > 0) {
        self->setProgress((int)((ctx->downloaded * 100) / ctx->totalSize),
                          ctx->downloaded, ctx->totalSize);
    }
    return true;
}

bool OTAA::downloadFirmware() {
    _hal->log('I', "OTAA", "Downloading firmware (streaming)...");
    _hal->log('I', "OTAA", "URL: %s, Size: %u bytes",
              _firmwareInfo.downloadUrl.c_str(), (unsigned)_firmwareInfo.fileSize);
    otaLog(String("[OTAA] Downloading firmware v") + _firmwareInfo.version.c_str()
           + ", size: " + String(_firmwareInfo.fileSize).c_str() + " bytes");

    size_t totalSize = 0;
    OtaStreamCtx ctx = { this, 0, 0, false };

    // 先 otaBegin（需要预估大小）
    if (!_hal->otaBegin(_firmwareInfo.fileSize > 0 ? _firmwareInfo.fileSize : 2 * 1024 * 1024)) {
        setError("OTA begin failed");
        return false;
    }

    if (_firmwareInfo.md5.length() > 0) {
        _hal->otaSetMD5(_firmwareInfo.md5.c_str());
    }

    // 流式下载：数据通过回调逐块写入 OTA 分区，不在内存中累积
    int status = _hal->httpGetStream(_firmwareInfo.downloadUrl.c_str(),
                                     _deviceToken.c_str(),
                                     otaStreamCallback, &ctx,
                                     &totalSize);

    if (status != 200 || ctx.failed) {
        setError(String("Download failed: HTTP ") + String(status).c_str());
        return false;
    }

    ctx.totalSize = totalSize > 0 ? totalSize : _firmwareInfo.fileSize;

    setState(OTA_INSTALLING);
    if (!_hal->otaEnd(_firmwareInfo.md5.length() > 0 ? _firmwareInfo.md5.c_str() : nullptr)) {
        setError("OTA end failed");
        return false;
    }

    _hal->log('I', "OTAA", "Firmware installed successfully (%u bytes)", (unsigned)ctx.downloaded);
    otaLog("[OTAA] Firmware installed successfully");
    return true;
}

// ========== HTTP 工具（委托给 HAL） ==========

String OTAA::httpGet(const String& url) {
    String response;
    int status = _hal->httpGet(url.c_str(),
                               _deviceToken.length() > 0 ? _deviceToken.c_str() : nullptr,
                               response);
    if (status != 200) {
        setError(String("HTTP GET failed: ") + String(status).c_str());
        return "";
    }
    return response;
}

String OTAA::httpPost(const String& url, const String& json) {
    String response;
    int status = _hal->httpPost(url.c_str(),
                                _deviceToken.length() > 0 ? _deviceToken.c_str() : nullptr,
                                json.c_str(), response);
    if (status != 200) {
        setError(String("HTTP POST failed: ") + String(status).c_str());
        return "";
    }
    return response;
}

String OTAA::httpPostMultipart(const String& url, const String& fieldName,
                                const uint8_t* data, size_t len, const String& filename) {
    _hal->log('I', "OTAA", "httpPostMultipart: url=%s field=%s file=%s size=%u",
              url.c_str(), fieldName.c_str(), filename.c_str(), (unsigned)len);
    String response;
    int status = _hal->httpPostMultipart(url.c_str(),
                                          _deviceToken.length() > 0 ? _deviceToken.c_str() : nullptr,
                                          fieldName.c_str(), data, len,
                                          filename.c_str(), response);
    _hal->log('I', "OTAA", "httpPostMultipart: HTTP %d, response=%s", status, response.c_str());
    if (status != 200) {
        setError(String("Upload failed: HTTP ") + String(status).c_str());
        return "";
    }
    return response;
}

// ========== 设备 ID ==========

String OTAA::generateDeviceId() {
    String chipId = _hal->getChipId();
    // 用 chipId 的 SHA256 前 8 字节作为 deviceId
    // mbedTLS 在两个框架下都可用
    byte shaResult[32] = {0};

#if defined(ESP32) || defined(ESP_IDF_VERSION)
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

    return String("esp32_") + String(hash);
}

// ========== 命令 ==========

void OTAA::onCommand(CommandCallback callback) { _commandCallback = callback; }

void OTAA::enableCommandDispatcher() {
    CommandDispatcher& dispatcher = CommandDispatcher::getInstance();
    dispatcher.setUserData(this);

    onCommand([this](int commandId, String command, String params) {
        CommandResult result = CommandDispatcher::getInstance().dispatch(commandId, command, params);
        // 异步命令（如录音）由 handler 自行 ack，不自动 ack
        if (!result.isAsync) {
            ackCommand(commandId, result.isSuccess, result.result, result.errorMsg);
        }
    });

    _hal->log('I', "OTAA", "CommandDispatcher enabled, %d handler(s): %s",
              dispatcher.getHandlerCount(), dispatcher.getRegisteredCommands().c_str());
}

bool OTAA::checkCommands() {
    if (!_initialized) return false;
    if (_currentCommandId > 0) return false;

    DeviceCommand cmd = fetchPendingCommand();
    if (cmd.id > 0) {
        _currentCommandId = cmd.id;
        _commandStartTime = _hal->millis();

        _hal->log('I', "OTAA", "Received command: %s (ID: %ld)",
                  cmd.command.c_str(), cmd.id);

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

    size_t capacity = response.length() * 2;
    if (capacity < 1024) capacity = 1024;
    DynamicJsonDocument doc(capacity);
    DeserializationError error = deserializeJson(doc, response);
    if (error) {
        _hal->log('E', "OTAA", "Pending command JSON parse error: %s", error.c_str());
        return cmd;
    }

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

    DynamicJsonDocument doc(512 + result.length() + errorMsg.length());
    doc["status"] = success ? 2 : 3;
    if (result.length() > 0) doc["result"] = result;
    if (errorMsg.length() > 0) doc["errorMsg"] = errorMsg;

    String json;
    serializeJson(doc, json);

    _hal->log('I', "OTAA", "ackCommand[%d] success=%d json=%s", commandId, success, json.c_str());

    String response = httpPost(url, json);
    bool ok = !response.isEmpty();

    if (!ok) {
        _hal->log('E', "OTAA", "ackCommand[%d] FAILED (empty response, lastError=%s)", commandId, _lastError.c_str());
    } else {
        _hal->log('I', "OTAA", "ackCommand[%d] OK response=%s", commandId, response.c_str());
    }

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
    if (!_initialized || _deviceToken.length() == 0) return false;

    String url = _serverUrl + "/api/device/ota/report";

    DynamicJsonDocument doc(256);
    doc["fromVersion"] = fromVersion;
    doc["toVersion"] = toVersion;
    doc["status"] = status;
    if (errorMsg.length() > 0) doc["errorMsg"] = errorMsg;

    String json;
    serializeJson(doc, json);

    _hal->log('I', "OTAA", "Reporting OTA result: %s (%s -> %s)",
              status.c_str(), fromVersion.c_str(), toVersion.c_str());

    String response = httpPost(url, json);
    if (response.isEmpty()) return false;

    DynamicJsonDocument respDoc(256);
    DeserializationError error = deserializeJson(respDoc, response);
    if (error) return false;

    return respDoc["success"].as<bool>();
}

// ========== 日志上报 ==========

bool OTAA::uploadLogs() {
    if (!_initialized || _deviceToken.length() == 0) return false;

    std::vector<String> logs = OTALogger::getInstance().getLogs();
    if (logs.empty()) return true;

    DynamicJsonDocument doc(4096);
    JsonArray logsArray = doc.createNestedArray("logs");
    for (const String& log : logs) {
        logsArray.add(log);
    }

    String json;
    serializeJson(doc, json);

    String url = _serverUrl + "/api/device/logs/upload";
    _hal->log('I', "OTAA", "Uploading %d logs...", (int)logs.size());

    String response = httpPost(url, json);
    if (response.isEmpty()) return false;

    DynamicJsonDocument respDoc(256);
    DeserializationError error = deserializeJson(respDoc, response);
    if (error) return false;

    if (respDoc["success"].as<bool>()) {
        _hal->log('I', "OTAA", "Logs uploaded successfully");
        otaLog(String("[OTAA] Logs uploaded, count: ") + String(logs.size()).c_str());
        OTALogger::getInstance().clearBuffer();
        _lastLogUploadTime = _hal->millis();
        return true;
    }

    return false;
}

bool OTAA::isLogBufferFull() {
    return OTALogger::getInstance().getBufferSize() >= LOG_BUFFER_SIZE;
}

void OTAA::syncTime() {
    if (!_hal) {
        _lastError = "OTAA not initialized (hal is null)";
        return;
    }
    _hal->syncTime(8 * 3600, "ntp.aliyun.com");
    if (time(nullptr) >= 1000000000) {
        _timeSynced = true;
        _hal->log('I', "OTAA", "NTP time synced");
    } else {
        _timeSynced = false;
        _lastError = "NTP timeout (10s)";
        _hal->log('W', "OTAA", "NTP time sync failed");
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
    _hal->log('E', "OTAA", "Error: %s", error.c_str());
    if (_errorCallback) _errorCallback(error);
}
