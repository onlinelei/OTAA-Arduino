#ifndef OTA_LOGGER_H
#define OTA_LOGGER_H

#include <Arduino.h>
#include <vector>
#include <mutex>
#include <time.h>

// 日志缓冲区配置
#define LOG_BUFFER_SIZE 50          // 最大缓存条数
#define LOG_UPLOAD_INTERVAL 20000   // 20秒上报间隔
#define MAX_UPLOAD_RETRIES 3        // 最大重试次数

/**
 * OTA 日志收集器（单例）
 * 
 * 功能：
 * - 拦截 Serial.println 输出
 * - 设备端拼接时间戳和版本号
 * - 环形缓冲区缓存日志
 * - 定时批量上报
 */
class OTALogger {
private:
    std::vector<String> _logBuffer;     // 日志缓冲区（存储完整日志行）
    std::mutex _logMutex;               // 互斥锁保护缓冲区
    String _fwVersion;                  // 固件版本号
    unsigned long _lastUploadTime;      // 上次上报时间
    int _uploadRetryCount;              // 上传重试次数
    bool _initialized;                  // 是否已初始化
    bool _bufferFull;                   // 缓冲区满标记（触发立即上报）
    std::function<bool()> _uploadCallback;  // 上传回调函数
    
    // 私有构造函数（单例模式）
    OTALogger() : _lastUploadTime(0), _uploadRetryCount(0), _initialized(false), _bufferFull(false) {}
    
public:
    // 获取单例实例
    static OTALogger& getInstance() {
        static OTALogger instance;
        return instance;
    }
    
    // 禁止拷贝和赋值
    OTALogger(const OTALogger&) = delete;
    OTALogger& operator=(const OTALogger&) = delete;
    
    /**
     * 初始化日志收集器
     * @param fwVersion 固件版本号
     */
    void begin(const String& fwVersion) {
        std::lock_guard<std::mutex> lock(_logMutex);
        _fwVersion = fwVersion;
        _initialized = true;
        _logBuffer.reserve(LOG_BUFFER_SIZE);  // 预分配内存
        Serial.println("[OTALogger] Initialized, buffer size: " + String(LOG_BUFFER_SIZE));
    }
    
    /**
     * 添加日志（已格式化的完整日志行）
     * @param formattedLine 已格式化的日志行，格式：时间戳 [版本号] 原始内容
     */
    void addLog(const String& formattedLine) {
        if (!_initialized) return;

        std::lock_guard<std::mutex> lock(_logMutex);

        // 自动添加时间戳前缀（如果行不以日期格式开头）
        String line = formattedLine;
        if (line.length() < 19 || line.charAt(4) != '-' || line.charAt(7) != '-') {
            time_t now;
            time(&now);
            struct tm* timeInfo = localtime(&now);
            char timeBuf[20];
            if (timeInfo && timeInfo->tm_year > 100) {
                strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", timeInfo);
            } else {
                snprintf(timeBuf, sizeof(timeBuf), "1970-01-01 00:00:00");
            }
            line = String(timeBuf) + " " + line;
        }

        _logBuffer.push_back(line);

        // 缓冲区满时标记需要上报（不在锁内调用上传，避免死锁）
        if (_logBuffer.size() >= LOG_BUFFER_SIZE) {
            Serial.println("[OTALogger] Buffer full, will upload on next cycle");
            _bufferFull = true;
        }
    }
    
    /**
     * 检查是否需要上报（定时上报）
     * @return 是否需要上报
     */
    bool shouldUpload() {
        if (!_initialized) return false;
        
        unsigned long now = millis();
        return (now - _lastUploadTime >= LOG_UPLOAD_INTERVAL);
    }
    
    /**
     * 上报日志（外部调用）
     * @return 是否上报成功
     */
    bool uploadLogs() {
        if (!_initialized) return false;
        
        std::lock_guard<std::mutex> lock(_logMutex);
        return uploadLogsInternal();
    }
    
    /**
     * 获取缓冲区大小
     * @return 缓冲区中的日志条数
     */
    size_t getBufferSize() {
        std::lock_guard<std::mutex> lock(_logMutex);
        return _logBuffer.size();
    }
    
    /**
     * 获取缓冲区中的所有日志（用于上报）
     * @return 日志列表
     */
    std::vector<String> getLogs() {
        std::lock_guard<std::mutex> lock(_logMutex);
        return _logBuffer;
    }
    
    /**
     * 清空缓冲区
     */
    void clearBuffer() {
        std::lock_guard<std::mutex> lock(_logMutex);
        _logBuffer.clear();
        _uploadRetryCount = 0;
        _bufferFull = false;
    }

    /**
     * 检查是否因缓冲区满而需要立即上报
     */
    bool isBufferFull() {
        std::lock_guard<std::mutex> lock(_logMutex);
        return _bufferFull;
    }
    
    /**
     * 设置上传回调函数
     * @param callback 上传回调函数，返回true表示上传成功
     */
    void setUploadCallback(std::function<bool()> callback) {
        _uploadCallback = callback;
    }

private:
    /**
     * 内部上报方法（调用时已持有锁）
     * @return 是否上报成功
     */
    bool uploadLogsInternal() {
        if (_logBuffer.empty()) return true;
        
        // 调用外部设置的上传回调
        if (_uploadCallback) {
            bool success = _uploadCallback();
            if (success) {
                _logBuffer.clear();
                _uploadRetryCount = 0;
                _lastUploadTime = millis();
            }
            return success;
        }
        
        return false;
    }
};

/**
 * Serial 输出拦截器
 * 继承 Print 类，拦截所有 Serial 输出
 * 在设备端拼接时间戳和版本号
 */
class SerialInterceptor : public Print {
private:
    Print& _original;   // 原始 Serial 输出
    String _lineBuffer; // 行缓冲区
    String _fwVersion;  // 固件版本号
    
public:
    SerialInterceptor(Print& original, const String& fwVersion) 
        : _original(original), _fwVersion(fwVersion) {}
    
    // 重写 write 方法，拦截所有输出
    size_t write(uint8_t c) override {
        // 1. 原样输出到串口（保持原有行为）
        _original.write(c);
        
        // 2. 收集到行缓冲区
        if (c == '\n') {
            // 行结束，拼接时间戳和版本号后保存
            if (_lineBuffer.length() > 0) {
                String formattedLine = formatLine(_lineBuffer);
                OTALogger::getInstance().addLog(formattedLine);
            }
            _lineBuffer = "";
        } else if (c != '\r') {
            _lineBuffer += (char)c;
        }
        
        return 1;
    }
    
private:
    /**
     * 拼接日志行格式：时间戳 [版本号] 原始内容
     * 示例：2024-01-15 10:30:00 [v1.5.9] [OTAA] Firmware is up to date
     */
    String formatLine(const String& content) {
        // 获取当前时间
        time_t now;
        time(&now);
        struct tm* timeInfo = localtime(&now);
        
        char timeBuf[20];
        strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", timeInfo);
        
        // 拼接：时间戳 [版本号] 原始内容
        return String(timeBuf) + " [v" + _fwVersion + "] " + content;
    }
};

// 全局日志函数（方便在任何地方调用）
inline void otaLog(const String& message) {
    OTALogger::getInstance().addLog(message);
}

#endif // OTA_LOGGER_H
