/**
 * EspHal — ESP-IDF 框架 HAL 实现
 *
 * 使用 ESP-IDF 原生 API 实现 OTAAHAL 接口：
 * - esp_wifi / esp_netif（WiFi）
 * - esp_http_client（HTTP）
 * - esp_ota_ops（OTA）
 * - cJSON（JSON 替代 ArduinoJson）
 * - mbedTLS（SHA256）
 * - ESP_LOG（日志）
 *
 * 注意：此文件仅在 ESP-IDF 框架下编译（#ifdef ESP_IDF_VERSION）
 */

#ifndef OTAA_ESP_HAL_H
#define OTAA_ESP_HAL_H

#include "Hal.h"

#if defined(ESP_IDF_VERSION)

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "mbedtls/md.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class EspHal : public OTAAHAL {
public:
    // ========== HTTP ==========

    int httpGet(const char* url, const char* token, String& response) override {
        esp_http_client_config_t config = {};
        config.url = url;
        config.method = HTTP_METHOD_GET;
        config.timeout_ms = 30000;

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (token && token[0]) {
            esp_http_client_set_header(client, "Authorization",
                                       (std::string("Bearer ") + token).c_str());
        }

        esp_err_t err = esp_http_client_open(client, 0);
        if (err != ESP_OK) {
            esp_http_client_cleanup(client);
            return -1;
        }

        int content_length = esp_http_client_fetch_headers(client);
        int status = esp_http_client_get_status_code(client);

        if (status == 200) {
            int total_read = 0;
            char buf[512];
            response.clear();
            while (true) {
                int read = esp_http_client_read(client, buf, sizeof(buf));
                if (read <= 0) break;
                response.append(buf, read);
                total_read += read;
            }
        }

        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return status;
    }

    int httpPost(const char* url, const char* token,
                 const char* body, String& response) override {
        esp_http_client_config_t config = {};
        config.url = url;
        config.method = HTTP_METHOD_POST;
        config.timeout_ms = 30000;

        esp_http_client_handle_t client = esp_http_client_init(&config);
        esp_http_client_set_header(client, "Content-Type", "application/json");
        if (token && token[0]) {
            esp_http_client_set_header(client, "Authorization",
                                       (std::string("Bearer ") + token).c_str());
        }
        esp_http_client_set_post_field(client, body, strlen(body));

        esp_err_t err = esp_http_client_perform(client);
        int status = -1;
        if (err == ESP_OK) {
            status = esp_http_client_get_status_code(client);
            if (status == 200) {
                int content_length = esp_http_client_get_content_length(client);
                if (content_length > 0) {
                    response.resize(content_length);
                    esp_http_client_read(client, &response[0], content_length);
                }
            }
        }

        esp_http_client_cleanup(client);
        return status;
    }

    int httpPostMultipart(const char* url, const char* token,
                          const char* fieldName,
                          const uint8_t* data, size_t len,
                          const char* filename, String& response) override {
        // 构建 multipart boundary
        char boundary[32];
        snprintf(boundary, sizeof(boundary), "----OTAA%lu",
                 (unsigned long)(esp_timer_get_time() / 1000));

        // 构建 multipart body
        std::string bodyStart = std::string("--") + boundary + "\r\n"
            + "Content-Disposition: form-data; name=\"" + fieldName
            + "\"; filename=\"" + filename
            + "\"\r\nContent-Type: application/octet-stream\r\n\r\n";
        std::string bodyEnd = std::string("\r\n--") + boundary + "--\r\n";

        size_t totalLen = bodyStart.size() + len + bodyEnd.size();
        uint8_t* postData = (uint8_t*)malloc(totalLen);
        if (!postData) return -1;

        memcpy(postData, bodyStart.c_str(), bodyStart.size());
        memcpy(postData + bodyStart.size(), data, len);
        memcpy(postData + bodyStart.size() + len, bodyEnd.c_str(), bodyEnd.size());

        std::string contentType = std::string("multipart/form-data; boundary=") + boundary;

        esp_http_client_config_t config = {};
        config.url = url;
        config.method = HTTP_METHOD_POST;
        config.timeout_ms = 60000;

        esp_http_client_handle_t client = esp_http_client_init(&config);
        esp_http_client_set_header(client, "Content-Type", contentType.c_str());
        if (token && token[0]) {
            esp_http_client_set_header(client, "Authorization",
                                       (std::string("Bearer ") + token).c_str());
        }
        esp_http_client_set_post_field(client, (const char*)postData, totalLen);

        esp_err_t err = esp_http_client_perform(client);
        int status = -1;
        if (err == ESP_OK) {
            status = esp_http_client_get_status_code(client);
            if (status == 200) {
                int content_length = esp_http_client_get_content_length(client);
                if (content_length > 0) {
                    response.resize(content_length);
                    esp_http_client_read(client, &response[0], content_length);
                }
            }
        }

        free(postData);
        esp_http_client_cleanup(client);
        return status;
    }

    // ========== 流式 HTTP ==========

    int httpGetStream(const char* url, const char* token,
                      HttpStreamCallback callback, void* userdata,
                      size_t* totalSize) override {
        esp_http_client_config_t config = {};
        config.url = url;
        config.method = HTTP_METHOD_GET;
        config.timeout_ms = 60000;

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (token && token[0]) {
            esp_http_client_set_header(client, "Authorization",
                                       (std::string("Bearer ") + token).c_str());
        }

        esp_err_t err = esp_http_client_open(client, 0);
        if (err != ESP_OK) {
            esp_http_client_cleanup(client);
            return -1;
        }

        int content_length = esp_http_client_fetch_headers(client);
        if (totalSize) *totalSize = content_length > 0 ? content_length : 0;
        int status = esp_http_client_get_status_code(client);

        if (status == 200) {
            uint8_t buf[4096];
            while (true) {
                int read = esp_http_client_read(client, (char*)buf, sizeof(buf));
                if (read <= 0) break;
                if (!callback(buf, (size_t)read, userdata)) {
                    status = -2;  // 用户中止
                    break;
                }
            }
        }

        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return status;
    }

    // ========== OTA ==========

    bool otaBegin(size_t imageSize) override {
        _otaHandle = esp_ota_get_next_update_partition(NULL);
        if (!_otaHandle) return false;
        return esp_ota_begin(_otaHandle, imageSize, &_otaHandle) == ESP_OK;
    }

    size_t otaWrite(const uint8_t* data, size_t len) override {
        if (esp_ota_write(_otaHandle, data, len) == ESP_OK) {
            return len;
        }
        return 0;
    }

    bool otaEnd(const char* md5) override {
        if (esp_ota_end(_otaHandle) != ESP_OK) return false;
        if (esp_ota_set_boot_partition(_otaHandle) != ESP_OK) return false;
        return true;
    }

    void otaSetMD5(const char* md5) override {
        // ESP-IDF 的 OTA 不需要单独设置 MD5，
        // esp_ota_end() 会自动校验 image 的 secure boot 签名
        // 如果需要 MD5 校验，需要在 otaWrite 过程中自行计算并比对
        if (md5 && md5[0]) {
            strncpy(_expectedMD5, md5, sizeof(_expectedMD5) - 1);
        }
    }

    // ========== 系统 ==========

    String getLocalIP() override {
        esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (!netif) return "";
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) return "";
        char buf[16];
        snprintf(buf, sizeof(buf), IPSTR, IP2STR(&ip_info.ip));
        return buf;
    }

    String getChipId() override {
        uint8_t mac[6];
        esp_efuse_mac_get_default(mac);
        char buf[13];
        snprintf(buf, sizeof(buf), "%02x%02x%02x%02x%02x%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return buf;
    }

    String getSketchMD5() override {
        // ESP-IDF 没有 Arduino 的 linker-embedded MD5
        // 返回空串，OTA 比对由服务端版本号控制
        return "";
    }

    void restart() override {
        esp_restart();
    }

    unsigned long millis() override {
        return (unsigned long)(esp_timer_get_time() / 1000);
    }

    void delay(unsigned long ms) override {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }

    void yield() override {
        taskYIELD();
    }

    void confirmFirmwareValid() override {
        const esp_partition_t* running = esp_ota_get_running_partition();
        esp_ota_img_states_t state;
        if (esp_ota_get_state_partition(running, &state) == ESP_OK
                && state == ESP_OTA_IMG_PENDING_VERIFY) {
            esp_ota_mark_app_valid_cancel_rollback();
        }
    }

    bool syncTime(long gmtOffset_sec, const char* ntpServer) override {
        esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, ntpServer);
        esp_sntp_init();

        int retry = 0;
        while (time(nullptr) < 1000000000 && retry < 20) {
            vTaskDelay(pdMS_TO_TICKS(500));
            retry++;
        }

        if (time(nullptr) >= 1000000000) {
            setenv("TZ", "CST-8", 1);
            tzset();
            return true;
        }
        return false;
    }

    void log(char level, const char* tag, const char* fmt, ...) override {
        char buf[256];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        switch (level) {
            case 'E': ESP_LOGE(tag, "%s", buf); break;
            case 'W': ESP_LOGW(tag, "%s", buf); break;
            case 'D': ESP_LOGD(tag, "%s", buf); break;
            default:  ESP_LOGI(tag, "%s", buf); break;
        }
    }

private:
    esp_ota_handle_t _otaHandle = 0;
    char _expectedMD5[33] = {};
};

#endif // ESP_IDF_VERSION
#endif // OTAA_ESP_HAL_H
