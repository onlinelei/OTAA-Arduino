/**
 * OTAA HAL (Hardware Abstraction Layer)
 *
 * 纯虚接口，定义 OTAA 核心业务所需的全部平台能力。
 * 框架适配层（ArduinoHal / EspHal）实现此接口即可。
 *
 * 不包含任何平台头文件，纯 C++ 接口。
 */

#ifndef OTAA_HAL_H
#define OTAA_HAL_H

#include <cstddef>
#include <cstdint>
#include <cstdarg>

// 跨框架字符串类型：Arduino 用 String，ESP-IDF 用兼容封装
#if defined(ARDUINO)
#include <Arduino.h>
#else
#include <string>
#include <sstream>

/**
 * String — Arduino String 兼容封装（基于 std::string）
 *
 * 提供 Arduino String 常用方法的 std::string 等价实现，
 * 让 OTAA.cpp/OTALogger.h 在 ESP-IDF 下零改动编译。
 */
class String : public std::string {
public:
    using std::string::string;  // 继承所有构造函数
    String() = default;
    String(const std::string& s) : std::string(s) {}
    String(const char* s) : std::string(s ? s : "") {}
    String(int val) : std::string(std::to_string(val)) {}
    String(long val) : std::string(std::to_string(val)) {}
    String(unsigned long val) : std::string(std::to_string(val)) {}
    String(size_t val) : std::string(std::to_string(val)) {}
    String(double val, int digits = 2) {
        std::ostringstream oss;
        oss.precision(digits);
        oss << std::fixed << val;
        assign(oss.str());
    }

    bool isEmpty() const { return empty(); }

    String substring(size_t from) const {
        return String(substr(from));
    }
    String substring(size_t from, size_t to) const {
        return String(substr(from, to - from));
    }

    bool endsWith(const String& suffix) const {
        if (suffix.size() > size()) return false;
        return compare(size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    bool startsWith(const String& prefix) const {
        if (prefix.size() > size()) return false;
        return compare(0, prefix.size(), prefix) == 0;
    }

    char charAt(size_t index) const { return at(index); }

    int indexOf(char c, size_t from = 0) const {
        auto pos = find(c, from);
        return pos == std::string::npos ? -1 : (int)pos;
    }

    int indexOf(const String& s, size_t from = 0) const {
        auto pos = find(s, from);
        return pos == std::string::npos ? -1 : (int)pos;
    }

    void remove(size_t index, size_t count = std::string::npos) {
        erase(index, count);
    }

    String& operator+=(const String& other) {
        append(other);
        return *this;
    }

    // 与 const char* 的 + 运算
    friend String operator+(const String& lhs, const char* rhs) {
        return String(static_cast<const std::string&>(lhs) + rhs);
    }
    friend String operator+(const char* lhs, const String& rhs) {
        return String(lhs + static_cast<const std::string&>(rhs));
    }
    friend String operator+(const String& lhs, const String& rhs) {
        return String(static_cast<const std::string&>(lhs) + static_cast<const std::string&>(rhs));
    }

    // ArduinoJson v7 需要 write() 方法（Arduino Print 接口）
    // 提供空实现让 ArduinoJson 的 Writer 模板匹配成功
    size_t write(uint8_t c) { push_back((char)c); return 1; }
    size_t write(const uint8_t* buf, size_t len) { append((const char*)buf, len); return len; }

    // bool 转换（用于 if(response) 检查和 return String → bool）
    operator bool() const { return !empty(); }
};
#endif

class OTAAHAL {
public:
    virtual ~OTAAHAL() {}

    // ========== HTTP ==========

    /**
     * HTTP GET 请求
     * @param url 完整 URL
     * @param token Bearer Token（可为空）
     * @param[out] response 响应体（成功时填充）
     * @return HTTP 状态码，失败返回负数
     */
    virtual int httpGet(const char* url, const char* token, String& response) = 0;

    /**
     * HTTP POST JSON 请求
     * @param url 完整 URL
     * @param token Bearer Token
     * @param body JSON 请求体
     * @param[out] response 响应体
     * @return HTTP 状态码
     */
    virtual int httpPost(const char* url, const char* token,
                         const char* body, String& response) = 0;

    /**
     * HTTP POST Multipart 文件上传
     * @param url 完整 URL
     * @param token Bearer Token
     * @param fieldName 表单字段名
     * @param data 文件数据
     * @param len 数据长度
     * @param filename 文件名
     * @param[out] response 响应体
     * @return HTTP 状态码
     */
    virtual int httpPostMultipart(const char* url, const char* token,
                                  const char* fieldName,
                                  const uint8_t* data, size_t len,
                                  const char* filename, String& response) = 0;

    // ========== 流式 HTTP ==========

    /**
     * 流式 HTTP GET — 用于固件下载等大文件场景
     * 数据通过回调逐块返回，不在内存中累积整个响应。
     *
     * @param url 完整 URL
     * @param token Bearer Token
     * @param callback 数据回调：(data, len) → 返回 true 继续，false 中止
     * @param userdata 透传给回调的用户指针
     * @param[out] totalSize 响应的 Content-Length（如果服务端提供了的话）
     * @return HTTP 状态码，失败返回负数
     */
    typedef bool (*HttpStreamCallback)(const uint8_t* data, size_t len, void* userdata);
    virtual int httpGetStream(const char* url, const char* token,
                              HttpStreamCallback callback, void* userdata,
                              size_t* totalSize) = 0;

    // ========== OTA ==========

    /**
     * 开始 OTA 写入
     * @param imageSize 固件总大小（字节）
     * @return 是否成功
     */
    virtual bool otaBegin(size_t imageSize) = 0;

    /**
     * 写入固件数据块
     * @param data 数据指针
     * @param len 数据长度
     * @return 实际写入字节数，失败返回 0
     */
    virtual size_t otaWrite(const uint8_t* data, size_t len) = 0;

    /**
     * 结束 OTA 写入并校验
     * @param md5 期望的 MD5（可为空跳过校验）
     * @return 是否成功
     */
    virtual bool otaEnd(const char* md5) = 0;

    /**
     * 设置 OTA MD5 校验（在 otaBegin 之后、otaWrite 之前调用）
     * @param md5 MD5 字符串
     */
    virtual void otaSetMD5(const char* md5) = 0;

    // ========== 系统 ==========

    /**
     * 获取本机 IP 地址
     * @return IP 字符串
     */
    virtual String getLocalIP() = 0;

    /**
     * 获取芯片唯一 ID
     * @return 芯片 ID 字符串
     */
    virtual String getChipId() = 0;

    /**
     * 获取当前运行固件的 MD5
     * @return MD5 字符串（ESP32 linker embedding，ESP8266 返回空）
     */
    virtual String getSketchMD5() = 0;

    /**
     * 重启设备
     */
    virtual void restart() = 0;

    /**
     * 获取毫秒时间戳（类似 Arduino millis()）
     */
    virtual unsigned long millis() = 0;

    /**
     * 延时毫秒
     */
    virtual void delay(unsigned long ms) = 0;

    /**
     * 让出 CPU 时间片（类似 Arduino yield()）
     */
    virtual void yield() = 0;

    /**
     * 确认固件有效（ESP32 A/B 回滚机制）
     * 在 begin() 时调用一次
     */
    virtual void confirmFirmwareValid() = 0;

    // ========== NTP ==========

    /**
     * 同步 NTP 时间
     * @param gmtOffset_sec GMT 偏移秒数（东八区 = 8*3600）
     * @param ntpServer NTP 服务器地址
     * @return 是否同步成功（超时 10s 内）
     */
    virtual bool syncTime(long gmtOffset_sec, const char* ntpServer) = 0;

    // ========== 日志 ==========

    /**
     * 输出日志（平台相关：Arduino Serial / ESP-IDF ESP_LOG）
     * @param level 日志级别（I=info, W=warn, E=error）
     * @param tag 标签
     * @param fmt 格式化字符串（printf 风格）
     */
    virtual void log(char level, const char* tag, const char* fmt, ...) = 0;
};

#endif // OTAA_HAL_H
