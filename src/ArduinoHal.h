/**
 * ArduinoHal — Arduino 框架 HAL 实现
 *
 * 使用 Arduino API（WiFi.h, HTTPClient.h, Update.h）实现 OTAAHAL 接口。
 * 支持 ESP32 和 ESP8266。
 */

#ifndef OTAA_ARDUINO_HAL_H
#define OTAA_ARDUINO_HAL_H

#include "Hal.h"

#if defined(ARDUINO)

#if defined(ESP32)
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <mbedtls/md.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClient.h>
#include <SHA256.h>
#endif

class ArduinoHal : public OTAAHAL {
public:
    // ========== HTTP ==========

    int httpGet(const char* url, const char* token, String& response) override {
#if defined(ESP32)
        HTTPClient http;
        http.begin(url);
        if (token && token[0]) {
            http.addHeader("Authorization", String("Bearer ") + token);
        }
        http.setTimeout(30000);
        int httpCode = http.GET();
        if (httpCode == 200) {
            response = http.getString();
        }
        http.end();
        return httpCode;
#elif defined(ESP8266)
        WiFiClient client;
        HTTPClient http;
        http.begin(client, url);
        if (token && token[0]) {
            http.addHeader("Authorization", String("Bearer ") + token);
        }
        http.setTimeout(30000);
        int httpCode = http.GET();
        if (httpCode == 200) {
            response = http.getString();
        }
        http.end();
        return httpCode;
#endif
    }

    int httpPost(const char* url, const char* token,
                 const char* body, String& response) override {
#if defined(ESP32)
        HTTPClient http;
        http.begin(url);
        http.addHeader("Content-Type", "application/json");
        if (token && token[0]) {
            http.addHeader("Authorization", String("Bearer ") + token);
        }
        http.setTimeout(30000);
        int httpCode = http.POST(body);
        if (httpCode == 200) {
            response = http.getString();
        }
        http.end();
        return httpCode;
#elif defined(ESP8266)
        WiFiClient client;
        HTTPClient http;
        http.begin(client, url);
        http.addHeader("Content-Type", "application/json");
        if (token && token[0]) {
            http.addHeader("Authorization", String("Bearer ") + token);
        }
        http.setTimeout(30000);
        int httpCode = http.POST(body);
        if (httpCode == 200) {
            response = http.getString();
        }
        http.end();
        return httpCode;
#endif
    }

    int httpPostMultipart(const char* url, const char* token,
                          const char* fieldName,
                          const uint8_t* data, size_t len,
                          const char* filename, String& response) override {
        String boundary = "----OTAA" + String(millis());

        // 构建 multipart body
        String bodyStart = "--" + boundary + "\r\n";
        bodyStart += "Content-Disposition: form-data; name=\"";
        bodyStart += fieldName;
        bodyStart += "\"; filename=\"";
        bodyStart += filename;
        bodyStart += "\"\r\nContent-Type: application/octet-stream\r\n\r\n";
        String bodyEnd = "\r\n--" + boundary + "--\r\n";

        size_t totalLen = bodyStart.length() + len + bodyEnd.length();
        uint8_t* postData = (uint8_t*)malloc(totalLen);
        if (!postData) return -1;

        memcpy(postData, bodyStart.c_str(), bodyStart.length());
        memcpy(postData + bodyStart.length(), data, len);
        memcpy(postData + bodyStart.length() + len, bodyEnd.c_str(), bodyEnd.length());

#if defined(ESP32)
        HTTPClient http;
        http.begin(url);
        if (token && token[0]) {
            http.addHeader("Authorization", String("Bearer ") + token);
        }
        http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
        int httpCode = http.POST(postData, totalLen);
        if (httpCode == 200) {
            response = http.getString();
        }
        free(postData);
        http.end();
        return httpCode;
#elif defined(ESP8266)
        WiFiClient client;
        HTTPClient http;
        http.begin(client, url);
        if (token && token[0]) {
            http.addHeader("Authorization", String("Bearer ") + token);
        }
        http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
        int httpCode = http.POST(postData, totalLen);
        if (httpCode == 200) {
            response = http.getString();
        }
        free(postData);
        http.end();
        return httpCode;
#endif
    }

    // ========== OTA ==========

    bool otaBegin(size_t imageSize) override {
        return Update.begin(imageSize);
    }

    size_t otaWrite(const uint8_t* data, size_t len) override {
        return Update.write(data, len);
    }

    bool otaEnd(const char* md5) override {
        return Update.end(true);
    }

    void otaSetMD5(const char* md5) override {
        if (md5 && md5[0]) {
            Update.setMD5(md5);
        }
    }

    // ========== 系统 ==========

    String getLocalIP() override {
        return WiFi.localIP().toString();
    }

    String getChipId() override {
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

    String getSketchMD5() override {
#if defined(ESP32)
        return ESP.getSketchMD5();
#else
        return "";
#endif
    }

    void restart() override {
        ESP.restart();
    }

    unsigned long millis() override {
        return ::millis();
    }

    void delay(unsigned long ms) override {
        ::delay(ms);
    }

    void yield() override {
        ::yield();
    }

    void confirmFirmwareValid() override {
#if defined(ESP32)
        const esp_partition_t* running = esp_ota_get_running_partition();
        esp_ota_img_states_t state;
        if (esp_ota_get_state_partition(running, &state) == ESP_OK
                && state == ESP_OTA_IMG_PENDING_VERIFY) {
            esp_ota_mark_app_valid_cancel_rollback();
        }
#endif
    }

    bool syncTime(long gmtOffset_sec, const char* ntpServer) override {
        configTime(gmtOffset_sec, 0, ntpServer);
        setenv("TZ", "CST-8", 1);
        tzset();

        int retry = 0;
        while (time(nullptr) < 1000000000 && retry < 20) {
            ::delay(500);
            retry++;
        }
        return time(nullptr) >= 1000000000;
    }

    void log(char level, const char* tag, const char* fmt, ...) override {
        char buf[256];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        Serial.printf("[OTAA][%c][%s] %s\n", level, tag, buf);
    }
};

#endif // ARDUINO
#endif // OTAA_ARDUINO_HAL_H
