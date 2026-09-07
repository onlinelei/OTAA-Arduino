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

// 跨框架字符串类型：Arduino 用 String，ESP-IDF 用 std::string
// 这里用 typedef 统一，HAL 实现层自行 include 对应头文件
#if defined(ARDUINO)
#include <Arduino.h>
#else
#include <string>
typedef std::string String;
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
