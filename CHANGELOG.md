# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [2.0.0] - 2026-09-07

### Changed — BREAKING: CORE + HAL 架构重构
- **全新架构**：OTAA 库拆分为「核心业务层」+「HAL 适配层」，参考 RadioLib 单仓库模式
  - `src/Hal.h` — OTAAHAL 纯虚接口（HTTP/OTA/系统三类共 18 个方法）
  - `src/ArduinoHal.h` — Arduino 框架适配（WiFi.h / HTTPClient / Update / Serial）
  - `src/EspHal.h` — ESP-IDF 框架适配（esp_wifi / esp_http_client / esp_ota_ops / ESP_LOG）
  - `OTAA.cpp` 核心业务全部改为 `hal->xxx()` 调用，零直接框架依赖
- **自动框架检测**：`#if defined(ARDUINO)` 选 ArduinoHal，`#if defined(ESP_IDF_VERSION)` 选 EspHal，用户代码零改动
- **支持自定义 HAL 注入**：`OTAA(OTAAHAL* hal)` 构造函数，可接入 Zephyr / Linux / 测试 Mock
- **ESP-IDF 构建支持**：新增 `CMakeLists.txt`（idf_component_register）和 `idf_component.yml`
- **OTALogger / CommandDispatcher 去 Arduino 依赖**：不再 include Arduino.h，纯 C++ 实现

### Fixed
- **`setCheckInterval()` 注释错误**：修正为"默认 1 分钟（60000ms）"

## [1.4.1] - 2026-09-04

### Fixed
- **NTP 默认服务器国内可达**：之前默认 `pool.ntp.org` + `time.nist.gov`，国内运营商常屏蔽 UDP 123，导致设备启动后时间同步项一直失败（myTV BootScreen 的 Time 项红色不绿）。改为默认 `ntp.aliyun.com` + `cn.pool.ntp.org`，超时从 5s 提到 10s，并去掉 `if (_timeSynced) return` 早返回（确保每次 `syncTime()` 都真正重试，不依赖标志位的脏状态），失败原因写入 `_lastError` 供上层调用方读取。

## [1.4.0] - 2026-09-04

### Added
- CommandDispatcher 命令自注册分发器：实现 CommandHandler 接口 + REGISTER_COMMAND 宏即可接收自定义命令，无需手动 onCommand 分发
- OTALogger 日志组件

## [1.3.0] - 2026-09-03

### Added
- 流式固件下载：无需完整缓冲区 malloc，降低内存占用
- ESP32 A/B 分区回滚确认（esp_ota_mark_app_valid_cancel_rollback）
- 凭证存储钩子：setCredentialStorage() 支持 NVS 持久化 deviceId/token
- 心跳支持 forceUpdate（服务器强制立即升级）

### Changed
- JSON 库迁移至 ArduinoJson v7（JsonDocument API）

## [1.2.0] - 2026-07-26

### Added
- 命令接收功能：设备可通过独立接口获取待执行命令
- 命令回调机制：onCommand(callback) 注册命令回调
- 命令上报功能：ackCommand() 上报执行结果
- 文件上传功能：uploadFile() 上传录音等文件
- 命令超时机制：默认5分钟超时自动标记失败
- 命令检查间隔配置：setCommandCheckInterval() 默认5秒

### Changed
- autoCheck() 方法现在包含命令检查逻辑
- 命令拉取与心跳分离，提高实时性

## [1.1.0] - 2026-07-26

### Fixed
- 设备ID生成算法与后端保持一致（使用SHA256哈希）
- 注册请求字段名与后端API一致（驼峰命名）
- 响应字段解析与后端返回格式一致
- HTTP请求在无token时不发送Authorization头

### Changed
- 设备ID格式从 `esp32_{chipId}` 改为 `esp32_{sha256_hash前16位}`
- 注册成功后从响应中获取设备ID（而非本地生成）
- 使用ArduinoJson构建请求JSON

## [1.0.0] - 2026-07-05

### Added
- Initial release
- Support for ESP32 and ESP8266
- Automatic firmware updates
- Progress tracking with callbacks
- MD5 verification
- Activation code based device registration
- Auto check for updates
- Arduino IDE support
- PlatformIO support
- Basic example
- Advanced example
- With Display example
- GitHub Actions CI/CD
- Documentation
