/**
 * OTAA - OTA Update Library for Arduino
 *
 * Easy-to-use OTA update library for ESP32/ESP8266 devices.
 * Connects to OTAA server for automatic firmware updates.
 *
 * GitHub: https://github.com/otaa-platform/OTAA-Arduino
 * Website: http://118.145.100.70
 *
 * v1.3.0: streaming download, A/B rollback, credential storage
 *
 * v1.4.0 changes:
 *   - CommandDispatcher: self-registering command handlers (REGISTER_COMMAND)
 *   - ArduinoJson v7 (JsonDocument)
 *   - Streaming firmware download (no full-buffer malloc)
 *   - ESP32 A/B rollback confirmation (esp_ota_mark_app_valid_cancel_rollback)
 *   - Heartbeat with forceUpdate support
 *   - Credential storage hook (NVS persistence)
 *
 * MIT License
 */

#ifndef OTAA_H
#define OTAA_H

#include <Arduino.h>

#if defined(ESP32)
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_ota_ops.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClient.h>
#endif

#include <ArduinoJson.h>
#include "OTALogger.h"

#if defined(ESP32)
#include <mbedtls/md.h>
#elif defined(ESP8266)
#include <SHA256.h>
#endif

// 版本号
#define OTAA_VERSION "1.4.0"

// OTA 状态枚举
enum OTAState {
    OTA_IDLE,           // 空闲
    OTA_CHECKING,       // 检查更新中
    OTA_UPDATE_AVAILABLE, // 有更新可用
    OTA_DOWNLOADING,    // 下载中
    OTA_INSTALLING,     // 安装中
    OTA_SUCCESS,        // 成功
    OTA_FAILED          // 失败
};

// 固件信息结构体
struct FirmwareInfo {
    bool updateAvailable;   // 是否有更新
    String version;         // 新版本号
    String downloadUrl;     // 下载地址
    size_t fileSize;        // 文件大小
    String md5;             // MD5校验
    String releaseNotes;    // 更新说明
};

// 设备命令结构体
struct DeviceCommand {
    long id;            // 命令ID
    String command;     // 命令类型
    String params;      // 命令参数 JSON
};

// 心跳响应结构体
struct HeartbeatResponse {
    bool forceUpdate;       // 是否强制更新
    String serverTime;      // 服务器时间
};

// 回调函数类型
typedef void (*OTAStateCallback)(OTAState state);
typedef void (*OTAProgressCallback)(int progress, size_t downloaded, size_t total);
typedef void (*OTAErrorCallback)(const String& error);
typedef void (*CommandCallback)(int commandId, String command, String params);

/**
 * 凭证存储接口
 * 用于持久化 deviceId / deviceToken，避免每次烧录都重新注册。
 * 实现示例（ESP32 NVS）：
 *
 *   void saveCredentials(const String& deviceId, const String& token) {
 *       Preferences p; p.begin("otaa", false);
 *       p.putString("device_id", deviceId);
 *       p.putString("device_token", token);
 *       p.end();
 *   }
 *   bool loadCredentials(String& deviceId, String& token) {
 *       Preferences p; p.begin("otaa", true);
 *       deviceId = p.getString("device_id", "");
 *       token = p.getString("device_token", "");
 *       p.end();
 *       return deviceId.length() > 0;
 *   }
 */
typedef void (*CredentialSaveCallback)(const String& deviceId, const String& token);
typedef bool (*CredentialLoadCallback)(String& deviceId, String& token);

class OTAA {
public:
    OTAA();
    ~OTAA();

    /**
     * 初始化 OTAA 库
     * @param serverUrl 服务器地址，如 "http://118.145.100.70"
     * @param deviceId 设备ID，首次联网自动生成
     * @param deviceToken 设备Token，注册后获得
     * @return 是否初始化成功
     */
    bool begin(const char* serverUrl, const char* deviceId, const char* deviceToken);

    /**
     * 初始化 OTAA 库（使用激活码）
     * @param serverUrl 服务器地址
     * @param activationCode 激活码
     * @return 是否初始化成功
     */
    bool beginWithActivationCode(const char* serverUrl, const char* activationCode);

    /**
     * 检查更新
     * @return 是否有更新可用
     */
    bool checkUpdate();

    /**
     * 执行更新
     * @return 是否更新成功
     */
    bool update();

    /**
     * 设置当前固件版本
     * @param version 版本号，如 "1.0.0"
     */
    void setFirmwareVersion(const char* version);

    /**
     * 设置检查更新间隔
     * @param intervalMs 间隔时间（毫秒），默认 1 分钟（60000）
     */
    void setCheckInterval(unsigned long intervalMs);

    /**
     * 设置命令检查间隔
     * @param intervalMs 间隔时间（毫秒），默认5秒
     */
    void setCommandCheckInterval(unsigned long intervalMs);

    /**
     * 设置命令超时时间
     * @param timeoutMs 超时时间（毫秒），默认5分钟
     */
    void setCommandTimeout(unsigned long timeoutMs);

    /**
     * 设置自动检查更新
     * @param enable 是否启用
     */
    void setAutoCheck(bool enable);

    /**
     * 设置心跳间隔
     * @param intervalMs 间隔时间（毫秒），默认60秒
     */
    void setHeartbeatInterval(unsigned long intervalMs);

    /**
     * 设置凭证存储回调（NVS 持久化）
     * 注册成功后自动调用 save，重启后自动调用 load。
     */
    void setCredentialStorage(CredentialSaveCallback save, CredentialLoadCallback load);

    /**
     * 自动检查更新（在loop中调用）
     * 包含：心跳 + 命令检查 + 固件更新检查
     * @return 是否检查了更新
     */
    bool autoCheck();

    /**
     * 发送心跳
     * @return 是否成功
     */
    bool heartbeat();

    /**
     * 获取当前状态
     */
    OTAState getState();

    /**
     * 获取下载进度 (0-100)
     */
    int getProgress();

    /**
     * 获取最后的错误信息
     */
    String getLastError();

    /**
     * 获取固件信息
     */
    FirmwareInfo getFirmwareInfo();

    /**
     * 获取设备ID
     */
    String getDeviceId();

    /**
     * 获取设备Token
     */
    String getDeviceToken();

    /**
     * 注册设备（使用激活码）
     * @return 是否注册成功
     */
    bool registerDevice();

    // ========== 命令相关 ==========

    /**
     * 注册命令回调
     * 当收到新命令时触发
     */
    void onCommand(CommandCallback callback);

    /**
     * 上报命令执行结果
     * @param commandId 命令ID
     * @param success 是否成功
     * @param result 执行结果 JSON
     * @param errorMsg 错误信息
     * @return 是否上报成功
     */
    bool ackCommand(int commandId, bool success, const String& result = "", const String& errorMsg = "");

    /**
     * 上传文件（录音等）
     * @param commandId 命令ID
     * @param fieldName 表单字段名
     * @param data 文件数据
     * @param len 数据长度
     * @param filename 文件名
     * @return 是否上传成功
     */
    bool uploadFile(int commandId, const String& fieldName, const uint8_t* data, size_t len, const String& filename);

    /**
     * 手动检查命令
     * @return 是否有新命令
     */
    bool checkCommands();

    // 回调函数设置
    void onStateChange(OTAStateCallback callback);
    void onProgress(OTAProgressCallback callback);
    void onError(OTAErrorCallback callback);

    // ========== 日志上报相关 ==========

    /**
     * 设置日志上报间隔
     * @param intervalMs 间隔时间（毫秒），默认5分钟
     */
    void setLogUploadInterval(unsigned long intervalMs);

    /**
     * 上报日志到服务器
     * @return 是否上报成功
     */
    bool uploadLogs();

    /**
     * 检查日志缓冲区是否已满
     * @return 是否已满
     */
    bool isLogBufferFull();

    /**
     * 同步NTP时间
     */
    void syncTime();

private:
    String _serverUrl;
    String _deviceId;
    String _deviceToken;
    String _activationCode;
    String _firmwareVersion;

    OTAState _state;
    int _progress;
    String _lastError;
    FirmwareInfo _firmwareInfo;

    unsigned long _checkInterval;
    unsigned long _lastCheckTime;
    bool _autoCheck;
    bool _initialized;

    // 心跳相关
    unsigned long _heartbeatInterval;
    unsigned long _lastHeartbeatTime;
    bool _forceUpdate;

    // 凭证存储
    CredentialSaveCallback _credentialSave;
    CredentialLoadCallback _credentialLoad;

    // 命令相关
    int _currentCommandId;
    unsigned long _commandStartTime;
    unsigned long _commandCheckInterval;
    unsigned long _commandTimeout;
    unsigned long _lastCommandCheckTime;
    CommandCallback _commandCallback;

    // 日志上报相关
    unsigned long _logUploadInterval;
    unsigned long _lastLogUploadTime;
    bool _timeSynced;

    OTAStateCallback _stateCallback;
    OTAProgressCallback _progressCallback;
    OTAErrorCallback _errorCallback;

    void setState(OTAState state);
    void setProgress(int progress, size_t downloaded = 0, size_t total = 0);
    void setError(const String& error);

    bool downloadFirmware();

    String httpGet(const String& url);
    String httpPost(const String& url, const String& json);
    String httpPostMultipart(const String& url, const String& fieldName,
                             const uint8_t* data, size_t len, const String& filename);

    String generateDeviceId();
    String getChipId();

    DeviceCommand fetchPendingCommand();

    void confirmFirmwareValid();
    void saveCredentials();
    bool loadCredentials();
    
    /**
     * 上报OTA结果到服务器
     * @param fromVersion 来源版本
     * @param toVersion 目标版本
     * @param status 状态：success, failed, downloading, installing
     * @param errorMsg 错误信息
     * @return 是否上报成功
     */
    bool reportOtaResult(const String& fromVersion, const String& toVersion, 
                         const String& status, const String& errorMsg = "");
};

#endif // OTAA_H
