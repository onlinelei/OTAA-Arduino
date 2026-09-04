# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed
- **`setCheckInterval()` 注释错误**：`OTAA.h` 之前写"默认 6 小时"，与 `OTAA.cpp:13` 实际 `_checkInterval(60 * 1000) // 1 minute` 不一致。修正为"默认 1 分钟（60000ms）"。纯文档修正，不影响 v1.4.1 已发布版本运行时行为。

### Added
- **OTA 时序与 forceUpdate 立即触发文档沉淀**：把分散在 `OTAA.cpp` / `docs/USAGE.md` / `ota-manager` 后端 `DeviceServiceImpl` / `FirmwareServiceImpl` 的事实统一写进 `docs/USAGE.md` 的"OTA 检查时序与立即触发"小节，给后续接入者一份权威参考，避免再次出现"OTA 默认是 5 分钟"这类估算错误。
  - 默认间隔表（OTA / 心跳 / 命令检查 / 日志上传）
  - 完整更新链路（`autoCheck` → `checkUpdate` → `SemanticVersion.hasUpdate` 严格 `>`）
  - `forceUpdate` 立即触发链路（控制台按钮 → `triggerCheckUpdate` → `force_update=true` → 下次心跳立即 `checkUpdate`）

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
