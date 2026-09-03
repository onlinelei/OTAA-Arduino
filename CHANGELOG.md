# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
