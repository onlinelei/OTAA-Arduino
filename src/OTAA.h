/**
 * OTAA - OTA Update & Device Management Library
 *
 * Multi-framework library for ESP32/ESP8266 devices.
 * Connects to OTAA server for automatic firmware updates, remote commands,
 * heartbeat, and log reporting.
 *
 * Supported frameworks:
 *   - Arduino (ESP32 / ESP8266) — auto-selects ArduinoHal
 *   - ESP-IDF — auto-selects EspHal
 *   - Custom — inject your own OTAAHAL implementation
 *
 * GitHub: https://github.com/otaa-platform/OTAA-Arduino
 * Website: http://118.145.100.70
 *
 * MIT License
 */

#ifndef OTAA_H
#define OTAA_H

#include <functional>

// 框架自动检测 + HAL 选择
#include "Hal.h"
#if defined(ESP_IDF_VERSION) && !defined(ARDUINO)
  #include "EspHal.h"
#elif defined(ARDUINO)
  #include "ArduinoHal.h"
#endif

#include "OTALogger.h"

// ArduinoJson：Arduino 框架直接用，ESP-IDF 框架需要 lib_deps 引入
#if defined(ARDUINO)
  #include <ArduinoJson.h>
#else
  // ESP-IDF 环境下 ArduinoJson 仍可用（通过 PlatformIO lib_deps），
  // 也可替换为 cJSON。此处保留 ArduinoJson 以保持代码一致性。
  #include <ArduinoJson.h>
#endif

// 版本号
#define OTAA_VERSION "2.0.0"

// OTA 状态枚举
enum OTAState {
    OTA_IDLE,               // 空闲
    OTA_CHECKING,           // 检查更新中
    OTA_UPDATE_AVAILABLE,   // 有更新可用
    OTA_DOWNLOADING,        // 下载中
    OTA_INSTALLING,         // 安装中
    OTA_SUCCESS,            // 成功
    OTA_FAILED              // 失败
};

// 固件信息结构体
struct FirmwareInfo {
    bool updateAvailable;
    String version;
    String downloadUrl;
    size_t fileSize;
    String md5;
    String releaseNotes;
};

// 设备命令结构体
struct DeviceCommand {
    long id;
    String command;
    String params;
};

// 心跳响应结构体
struct HeartbeatResponse {
    bool forceUpdate;
    String serverTime;
};

// 回调函数类型
typedef void (*OTAStateCallback)(OTAState state);
typedef void (*OTAProgressCallback)(int progress, size_t downloaded, size_t total);
typedef void (*OTAErrorCallback)(const String& error);
typedef std::function<void(int commandId, String command, String params)> CommandCallback;

// 凭证存储回调
typedef void (*CredentialSaveCallback)(const String& deviceId, const String& token);
typedef bool (*CredentialLoadCallback)(String& deviceId, String& token);

class OTAA {
public:
    /**
     * 构造函数（自动创建默认 HAL）
     * Arduino 框架下自动使用 ArduinoHal
     * ESP-IDF 框架下自动使用 EspHal
     */
    OTAA();

    /**
     * 构造函数（注入自定义 HAL）
     * 用于 Zephyr、Linux、测试等场景
     * @param hal 自定义 HAL 实现（OTAA 不拥有此指针，调用方负责生命周期）
     */
    explicit OTAA(OTAAHAL* hal);

    ~OTAA();

    // ========== 初始化 ==========

    bool begin(const char* serverUrl, const char* deviceId, const char* deviceToken);
    bool beginWithActivationCode(const char* serverUrl, const char* activationCode);

    // ========== 配置 ==========

    void setFirmwareVersion(const char* version);
    void setCheckInterval(unsigned long intervalMs);
    void setCommandCheckInterval(unsigned long intervalMs);
    void setCommandTimeout(unsigned long timeoutMs);
    void setAutoCheck(bool enable);
    void setHeartbeatInterval(unsigned long intervalMs);
    void setCredentialStorage(CredentialSaveCallback save, CredentialLoadCallback load);
    void setLogUploadInterval(unsigned long intervalMs);

    // ========== 主循环 ==========

    bool autoCheck();
    bool heartbeat();
    bool checkUpdate();
    bool update();

    // ========== 命令 ==========

    void onCommand(CommandCallback callback);
    void enableCommandDispatcher();
    bool ackCommand(int commandId, bool success, const String& result = "", const String& errorMsg = "");
    bool uploadFile(int commandId, const String& fieldName, const uint8_t* data, size_t len, const String& filename);
    bool checkCommands();

    // ========== 日志 ==========

    bool uploadLogs();
    bool isLogBufferFull();
    void syncTime();

    // ========== 状态查询 ==========

    OTAState getState();
    int getProgress();
    String getLastError();
    FirmwareInfo getFirmwareInfo();
    String getDeviceId();
    String getDeviceToken();
    OTAAHAL* getHal() { return _hal; }  // 供流式回调等内部使用

    // 回调注册
    void onStateChange(OTAStateCallback callback);
    void onProgress(OTAProgressCallback callback);
    void onError(OTAErrorCallback callback);

    // ========== 注册设备 ==========

    bool registerDevice();

private:
    // HAL 指针
    OTAAHAL* _hal;
    bool _ownsHal;  // 是否拥有 HAL（自动创建时为 true，外部注入时为 false）

    // 配置
    String _serverUrl;
    String _deviceId;
    String _deviceToken;
    String _activationCode;
    String _firmwareVersion;
    String _firmwareMd5;

    // 状态
    OTAState _state;
    int _progress;
    String _lastError;
    FirmwareInfo _firmwareInfo;

    // 定时器
    unsigned long _checkInterval;
    unsigned long _lastCheckTime;
    bool _autoCheck;
    bool _initialized;

    // 心跳
    unsigned long _heartbeatInterval;
    unsigned long _lastHeartbeatTime;
    bool _forceUpdate;

    // 凭证存储
    CredentialSaveCallback _credentialSave;
    CredentialLoadCallback _credentialLoad;

    // 命令
    int _currentCommandId;
    unsigned long _commandStartTime;
    unsigned long _commandCheckInterval;
    unsigned long _commandTimeout;
    unsigned long _lastCommandCheckTime;
    CommandCallback _commandCallback;

    // 1 深待执行槽：执行中收到新命令、而当前 handler 又无法让位时记下它，
    // 等当前命令结束（ack）后立刻执行。避免"新命令直接丢掉"。
    //
    // ⚠️ 这里**只记 id、不存 payload**。payload 由平台在每次轮询时原样重发
    //    （见服务端 getPendingCommand 步骤③）。这样做的关键收益：槽里那条一旦
    //    被平台 replace 取消，设备手里就不会还捧着一条已作废的命令继续跑
    //    （旧设计存 payload，替换后设备会先播旧的、再播新的）。
    int _pendingCommandId;
    // 执行中的命令是否为**异步**命令（execute() 返回 asyncStarted()）。
    // 异步命令天然可能跑很久（如 play_audio 长达 300s），看门狗必须区别对待。
    bool _currentCommandIsAsync;

    // 日志
    unsigned long _logUploadInterval;
    unsigned long _lastLogUploadTime;
    bool _timeSynced;

    // 回调
    OTAStateCallback _stateCallback;
    OTAProgressCallback _progressCallback;
    OTAErrorCallback _errorCallback;

    // 内部方法
    void setState(OTAState state);
    void setProgress(int progress, size_t downloaded = 0, size_t total = 0);
    void setError(const String& error);

    // 流式下载回调（需要访问 setProgress）
    friend bool otaStreamCallback(const uint8_t* data, size_t len, void* userdata);

    bool downloadFirmware();
    String httpGet(const String& url);
    String httpPost(const String& url, const String& json);
    String httpPostMultipart(const String& url, const String& fieldName,
                             const uint8_t* data, size_t len, const String& filename);

    String generateDeviceId();
    /**
     * 取一条待执行命令。
     * @param keepAliveCommandId 设备当前正在执行的命令 id（0 = 没有）。
     * @param queuedCommandId    设备槽里暂存等待的命令 id（0 = 没有）。
     *        两个 id 都会作为查询参数上报，平台据此**刷新对应命令的 executedAt**
     *        （保活），用来区分"这条命令在设备手上、正在处理"和"命令丢了 /
     *        设备死了"——没有这个信号，平台只能等满 timeoutSeconds(默认 300s)
     *        才敢释放，期间**拒绝下发任何新命令**（真机症状：一次意外 =
     *        后续 5 分钟全黑）。
     */
    DeviceCommand fetchPendingCommand(int keepAliveCommandId = 0, int queuedCommandId = 0);

    void confirmFirmwareValid();
    void computeFirmwareMD5();
    void saveCredentials();
    bool loadCredentials();

    bool reportOtaResult(const String& fromVersion, const String& toVersion,
                         const String& status, const String& errorMsg = "");
};

#endif // OTAA_H
