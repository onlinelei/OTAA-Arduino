/**
 * OTAA - OTA Update Library for Arduino
 *
 * Easy-to-use OTA update library for ESP32/ESP8266 devices.
 * Connects to OTAA server for automatic firmware updates.
 *
 * GitHub: https://github.com/otaa-platform/OTAA-Arduino
 * Website: http://118.145.100.70
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
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClient.h>
#endif

#include <ArduinoJson.h>

// 版本号
#define OTAA_VERSION "1.0.0"

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

// 回调函数类型
typedef void (*OTAStateCallback)(OTAState state);
typedef void (*OTAProgressCallback)(int progress, size_t downloaded, size_t total);
typedef void (*OTAErrorCallback)(const String& error);

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
     * @param intervalMs 间隔时间（毫秒），默认6小时
     */
    void setCheckInterval(unsigned long intervalMs);

    /**
     * 设置自动检查更新
     * @param enable 是否启用
     */
    void setAutoCheck(bool enable);

    /**
     * 自动检查更新（在loop中调用）
     * @return 是否检查了更新
     */
    bool autoCheck();

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

    // 回调函数设置
    void onStateChange(OTAStateCallback callback);
    void onProgress(OTAProgressCallback callback);
    void onError(OTAErrorCallback callback);

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

    OTAStateCallback _stateCallback;
    OTAProgressCallback _progressCallback;
    OTAErrorCallback _errorCallback;

    void setState(OTAState state);
    void setProgress(int progress, size_t downloaded = 0, size_t total = 0);
    void setError(const String& error);

    bool downloadFirmware();
    bool installFirmware(uint8_t* data, size_t len);

    String httpGet(const String& url);
    String httpPost(const String& url, const String& json);

    String generateDeviceId();
    String getChipId();
};

#endif // OTAA_H
