# OTAA Arduino Library Usage Guide

## Table of Contents

1. [Getting Started](#getting-started)
2. [Basic Usage](#basic-usage)
3. [Advanced Usage](#advanced-usage)
4. [Configuration](#configuration)
5. [Callbacks](#callbacks)
6. [Error Handling](#error-handling)
7. [Best Practices](#best-practices)
8. [Troubleshooting](#troubleshooting)
9. [OTA 检查时序与立即触发](#ota-检查时序与立即触发)

## Getting Started

### Prerequisites

- Arduino IDE 1.8+ or PlatformIO
- ESP32 or ESP8266 board
- WiFi connection
- OTAA account (http://118.145.100.70)

### Step 1: Get Activation Code

1. Visit http://118.145.100.70
2. Register an account
3. Go to "激活码管理" (Activation Codes)
4. Click "生成激活码" (Generate)
5. Copy the activation code (e.g., "VYOTDPYB")

### Step 2: Install Library

**Arduino IDE:**
1. Sketch → Include Library → Manage Libraries
2. Search "OTAA"
3. Click Install

**PlatformIO:**
```ini
lib_deps = otaa/OTAA@^1.0.0
```

### Step 3: Write Your Code

```cpp
#include <WiFi.h>
#include <OTAA.h>

const char* ssid = "your-wifi";
const char* password = "your-password";
const char* serverUrl = "http://118.145.100.70";
const char* activationCode = "YOUR_CODE";

#define FIRMWARE_VERSION "1.0.0"

OTAA ota;

void setup() {
    Serial.begin(115200);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }

    ota.setFirmwareVersion(FIRMWARE_VERSION);
    ota.beginWithActivationCode(serverUrl, activationCode);

    if (ota.checkUpdate()) {
        ota.update();
    }
}

void loop() {
    ota.autoCheck();
    delay(1000);
}
```

## Basic Usage

### Initialize with Activation Code

```cpp
OTAA ota;
ota.setFirmwareVersion("1.0.0");

if (ota.beginWithActivationCode("http://118.145.100.70", "YOUR_CODE")) {
    Serial.println("Ready!");
}
```

### Initialize with Device Token

```cpp
OTAA ota;
ota.setFirmwareVersion("1.0.0");

if (ota.begin("http://118.145.100.70", "device_id", "device_token")) {
    Serial.println("Ready!");
}
```

### Check for Updates

```cpp
if (ota.checkUpdate()) {
    Serial.println("Update available!");
    FirmwareInfo info = ota.getFirmwareInfo();
    Serial.println("New version: " + info.version);
    Serial.println("Size: " + String(info.fileSize) + " bytes");
}
```

### Perform Update

```cpp
if (ota.checkUpdate()) {
    if (ota.update()) {
        Serial.println("Update successful! Restarting...");
    } else {
        Serial.println("Update failed: " + ota.getLastError());
    }
}
```

### Auto Check

```cpp
void loop() {
    // Check every 6 hours (default)
    ota.autoCheck();

    // Your code here
    delay(1000);
}
```

## Advanced Usage

### Custom Check Interval

```cpp
// Check every hour
ota.setCheckInterval(3600000);

// Check every 30 minutes
ota.setCheckInterval(1800000);
```

### Disable Auto Check

```cpp
ota.setAutoCheck(false);
```

### Manual Check in Loop

```cpp
unsigned long lastCheck = 0;
unsigned long checkInterval = 3600000; // 1 hour

void loop() {
    if (millis() - lastCheck >= checkInterval) {
        lastCheck = millis();

        if (ota.checkUpdate()) {
            // Ask user before updating
            Serial.println("Update available! Update now? (y/n)");
            // ... handle user input
        }
    }
}
```

## OTA 检查时序与立即触发

> 这节把分散在 `OTAA.cpp` 的事实统一写出来，给做集成 / 排查的人一份权威参考，避免重复"OTA 默认是 5 分钟"这类估算错误。

### 默认间隔表（库层面默认值）

| 任务 | 间隔 | 字段 | 出处 |
|------|------|------|------|
| OTA 检查 | **60 秒（1 分钟）** | `_checkInterval` | `src/OTAA.cpp:13` |
| 心跳 | 60 秒 | `_heartbeatInterval` | `src/OTAA.cpp:17` |
| 命令检查 | 5 秒 | `_commandCheckInterval` | `src/OTAA.cpp:24` |
| 日志上传 | 5 分钟 | `_logUploadInterval` | `src/OTAA.h:285` 注释 |

可以在 `begin()` 之后用 `setXxxInterval()` 修改。

### 一条新固件从发布到落地的完整链路

```
平台 publish v1.7.0  →  platforms.latest = 1.7.0
                                │
设备 loop() (≤60s 内)           │
  ├─ heartbeat()                 │   POST /api/device/heartbeat
  │     body: {fwVersion: "1.6.99", ipAddress: "..."}
  │                              │
  └─ checkUpdate()               │   GET  /api/device/ota/check?current_version=1.6.99
            │                    │
            └────────────────────┴→  后端 FirmwareServiceImpl.checkUpdate
                                            └→ SemanticVersion.hasUpdate("1.6.99", "1.7.0")
                                                 (严格 latest > current)
                                                 → true
                                          返回 { updateAvailable: true,
                                                 version: "1.7.0",
                                                 downloadUrl: "...",
                                                 fileMd5: "...",
                                                 releaseNotes: "..." }
                       │
                       └→ update()  下载 → MD5 校验 → 切换分区重启
```

**关键判定**：`ota-manager` 后端的 `SemanticVersion.hasUpdate(current, latest)` 是严格 **`latest.compareTo(current) > 0`**——也就是说"设备的当前版本号 **必须严格小于** 平台 latest"才会更新。这是为什么历史残留的 `1.6.99` 卡住 OTA、必须发 `1.7.0` 跨过去的根因。

### forceUpdate：立即触发 OTA 检查（不等 1 分钟）

服务器可以在心跳响应里下发 `forceUpdate: true`，让设备**下次心跳就立刻**做 ota/check，不等默认的 60 秒。

链路：

```
控制台 (设备列表/详情页"立即触发"按钮)
        │ POST /api/devices/{deviceId}/trigger-check
        ▼
后端 DeviceController.triggerCheckUpdate
        │
        └→ DeviceServiceImpl.triggerCheckUpdate
                device.setForceUpdate(true)
                deviceMapper.updateById(device)
                          │
                          ▼
设备心跳                         │
        POST /api/device/heartbeat → 服务器读到 force_update=true → 重置为 false
                          │
        响应 { forceUpdate: true, serverTime: "..." }
                          │
                          ▼
设备 autoCheck() 处理心跳响应 (OTAA.cpp:172)
        if (_forceUpdate) { _lastCheckTime = 0; }   // 立即触发
                          │
                          ▼
        下一次 loop() 进入 ota/check 分支 → checkUpdate() → update()
```

控制台前端已经接好（`ota-frontend/src/views/console/devices/{list,detail}.vue` 都 import 了 `triggerCheckUpdate`，详见 `ota-frontend/src/api/devices.ts`）。

**实战用法**：发布完新固件想立刻验证 → 去设备列表点"立即触发" → 设备下次心跳回来就升级，**几秒**生效，不用干等 60 秒。

## Callbacks

### State Change Callback

```cpp
void onStateChange(OTAState state) {
    switch (state) {
        case OTA_IDLE:
            Serial.println("Idle");
            break;
        case OTA_CHECKING:
            Serial.println("Checking for updates...");
            break;
        case OTA_UPDATE_AVAILABLE:
            Serial.println("Update available!");
            break;
        case OTA_DOWNLOADING:
            Serial.println("Downloading...");
            break;
        case OTA_INSTALLING:
            Serial.println("Installing...");
            break;
        case OTA_SUCCESS:
            Serial.println("Success!");
            break;
        case OTA_FAILED:
            Serial.println("Failed!");
            break;
    }
}

// Register callback
ota.onStateChange(onStateChange);
```

### Progress Callback

```cpp
void onProgress(int progress, size_t downloaded, size_t total) {
    Serial.printf("Progress: %d%% (%d/%d bytes)\n", progress, downloaded, total);
}

// Register callback
ota.onProgress(onProgress);
```

### Error Callback

```cpp
void onError(const String& error) {
    Serial.println("Error: " + error);
}

// Register callback
ota.onError(onError);
```

## Error Handling

### Get Last Error

```cpp
if (!ota.checkUpdate()) {
    Serial.println("Error: " + ota.getLastError());
}
```

### Common Errors

| Error | Cause | Solution |
|-------|-------|----------|
| "Not initialized" | `begin()` not called | Call `begin()` first |
| "HTTP GET failed: 401" | Invalid token | Re-register device |
| "HTTP GET failed: 404" | Wrong server URL | Check URL |
| "Memory allocation failed" | Not enough RAM | Reduce buffer size |
| "Download failed" | Network issue | Check WiFi |
| "Update begin failed" | Flash issue | Check flash size |

## Best Practices

### 1. Always Check WiFi Before OTA

```cpp
void setup() {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }

    // Now safe to do OTA
    ota.begin(...);
}
```

### 2. Use Callbacks for UI Updates

```cpp
void onProgress(int progress, size_t downloaded, size_t total) {
    // Update display
    display.clear();
    display.printf("Downloading: %d%%", progress);
    display.display();
}
```

### 3. Handle Errors Gracefully

```cpp
if (ota.checkUpdate()) {
    if (!ota.update()) {
        Serial.println("Update failed: " + ota.getLastError());
        // Don't restart, try again later
    }
}
```

### 4. Set Reasonable Check Interval

```cpp
// Don't check too frequently
ota.setCheckInterval(6 * 60 * 60 * 1000); // 6 hours
```

### 5. Use LED for Status

```cpp
void onStateChange(OTAState state) {
    switch (state) {
        case OTA_CHECKING:
            digitalWrite(LED_BUILTIN, HIGH);
            break;
        case OTA_SUCCESS:
            // LED off before restart
            digitalWrite(LED_BUILTIN, LOW);
            break;
        case OTA_FAILED:
            // Blink LED
            for (int i = 0; i < 5; i++) {
                digitalWrite(LED_BUILTIN, HIGH);
                delay(100);
                digitalWrite(LED_BUILTIN, LOW);
                delay(100);
            }
            break;
    }
}
```

## Troubleshooting

### Update Not Found

1. Check `FIRMWARE_VERSION` matches your code
2. Verify firmware uploaded to OTAA server
3. Check device is registered (see device ID in console)
4. Wait for next check interval

### Download Fails

1. Check WiFi connection
2. Verify server URL
3. Check Serial Monitor for errors
4. Try increasing timeout

### Installation Fails

1. Verify firmware binary is for correct chip
2. Check available flash space
3. Ensure binary is not corrupted
4. Check MD5 checksum

### Device Not Registering

1. Check activation code is correct
2. Verify server is accessible
3. Check Serial Monitor for errors
4. Try generating new activation code

## API Reference

See [README.md](../README.md) for complete API reference.
