#ifndef OTA_LOGGER_H
#define OTA_LOGGER_H

#include "Hal.h"  // 提供 String 类型定义
#include <vector>
#include <mutex>
#include <time.h>

// 日志缓冲区配置
#define LOG_BUFFER_SIZE 50
#define LOG_UPLOAD_INTERVAL 20000
#define MAX_UPLOAD_RETRIES 3

/**
 * OTA 日志收集器（单例，平台无关）
 */
class OTALogger {
private:
    std::vector<String> _logBuffer;
    std::mutex _logMutex;
    String _fwVersion;
    unsigned long _lastUploadTime;
    int _uploadRetryCount;
    bool _initialized;
    bool _bufferFull;
    std::function<bool()> _uploadCallback;

    OTALogger() : _lastUploadTime(0), _uploadRetryCount(0), _initialized(false), _bufferFull(false) {}

public:
    static OTALogger& getInstance() {
        static OTALogger instance;
        return instance;
    }

    OTALogger(const OTALogger&) = delete;
    OTALogger& operator=(const OTALogger&) = delete;

    void begin(const String& fwVersion) {
        std::lock_guard<std::mutex> lock(_logMutex);
        _fwVersion = fwVersion;
        _initialized = true;
        _logBuffer.reserve(LOG_BUFFER_SIZE);
    }

    void addLog(const String& formattedLine) {
        if (!_initialized) return;

        std::lock_guard<std::mutex> lock(_logMutex);

        // 自动添加时间戳前缀
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

        if (_logBuffer.size() >= LOG_BUFFER_SIZE) {
            _bufferFull = true;
        }
    }

    bool shouldUpload() {
        if (!_initialized) return false;
        // 注意：这里不能直接调 millis()，需要外部判断
        return _bufferFull;
    }

    size_t getBufferSize() {
        std::lock_guard<std::mutex> lock(_logMutex);
        return _logBuffer.size();
    }

    std::vector<String> getLogs() {
        std::lock_guard<std::mutex> lock(_logMutex);
        return _logBuffer;
    }

    void clearBuffer() {
        std::lock_guard<std::mutex> lock(_logMutex);
        _logBuffer.clear();
        _uploadRetryCount = 0;
        _bufferFull = false;
    }

    bool isBufferFull() {
        std::lock_guard<std::mutex> lock(_logMutex);
        return _bufferFull;
    }

    void setUploadCallback(std::function<bool()> callback) {
        _uploadCallback = callback;
    }
};

/**
 * 全局日志函数
 * 注意：在 ESP-IDF 下 String 构造方式不同，这里用 const char* 重载
 */
inline void otaLog(const String& message) {
    OTALogger::getInstance().addLog(message);
}

#endif // OTA_LOGGER_H
