/**
 * CommandDispatcher - 自定义命令分发器
 *
 * 提供命令自注册机制，设备端开发者只需:
 * 1. 实现 CommandHandler 接口
 * 2. 使用 REGISTER_COMMAND 宏注册
 * 3. OTAA 库自动分发命令到对应 handler
 *
 * 示例:
 *
 *   class PlayAudioHandler : public CommandHandler {
 *   public:
 *       String getCommand() const override { return "play_audio"; }
 *       CommandResult execute(int id, const String& params) override {
 *           JsonDocument doc;
 *           deserializeJson(doc, params);
 *           int volume = doc["volume"] | 80;
 *           String url = doc["url"] | "";
 *           // ... 播放逻辑
 *           return CommandResult::success();
 *       }
 *   };
 *   REGISTER_COMMAND(PlayAudioHandler);
 *
 * OTAA 库在收到命令时自动调用 dispatcher.dispatch()，
 * 找不到对应 handler 时自动打印警告日志并上报失败。
 */

#ifndef COMMAND_DISPATCHER_H
#define COMMAND_DISPATCHER_H

#include <Arduino.h>

// 前向声明
class OTAA;

// 命令执行结果
struct CommandResult {
    bool isSuccess;
    String result;      // 结果 JSON
    String errorMsg;    // 错误信息

    static CommandResult success(const String& result = "") {
        return {true, result, ""};
    }

    static CommandResult failure(const String& errorMsg) {
        return {false, "", errorMsg};
    }
};

// 命令处理器接口
class CommandHandler {
public:
    virtual ~CommandHandler() {}

    // 返回此 handler 处理的命令编码 (如 "play_audio")
    virtual String getCommand() const = 0;

    // 执行命令
    // @param commandId 命令ID
    // @param params 命令参数 JSON 字符串
    // @return 执行结果
    virtual CommandResult execute(int commandId, const String& params) = 0;
};

// 命令分发器（单例）
class CommandDispatcher {
private:
    static const int MAX_HANDLERS = 32;

    CommandHandler* _handlers[MAX_HANDLERS];
    int _count;
    void* _userData;  // 用户数据指针（可用于存储 OTAA 引用等）

    CommandDispatcher() : _count(0), _userData(nullptr) {
        for (int i = 0; i < MAX_HANDLERS; i++) {
            _handlers[i] = nullptr;
        }
    }

public:
    static CommandDispatcher& getInstance() {
        static CommandDispatcher instance;
        return instance;
    }

    // 设置用户数据指针
    void setUserData(void* data) { _userData = data; }

    // 获取用户数据指针
    void* getUserData() const { return _userData; }

    // 获取 OTAA 实例指针（需要在使用前调用 setUserData）
    OTAA* getOTAAPtr() {
        if (!_userData) {
            Serial.println("[CommandDispatcher] ERROR: OTAA not set, call setUserData first");
            return nullptr;
        }
        return static_cast<OTAA*>(_userData);
    }

    // 注册命令处理器
    bool registerHandler(CommandHandler* handler) {
        if (_count >= MAX_HANDLERS) {
            Serial.println("[CommandDispatcher] 达到最大处理器数量限制");
            return false;
        }
        if (!handler) return false;

        String cmd = handler->getCommand();
        Serial.printf("[CommandDispatcher] 注册命令: %s\n", cmd.c_str());
        _handlers[_count++] = handler;
        return true;
    }

    // 分发命令
    // @param commandId 命令ID
    // @param command 命令编码
    // @param params 命令参数 JSON
    // @return 执行结果，找不到 handler 时返回 failure
    CommandResult dispatch(int commandId, const String& command, const String& params) {
        for (int i = 0; i < _count; i++) {
            if (_handlers[i] && _handlers[i]->getCommand() == command) {
                Serial.printf("[CommandDispatcher] 分发命令: %s (id=%d)\n",
                              command.c_str(), commandId);
                return _handlers[i]->execute(commandId, params);
            }
        }

        // 找不到对应 handler
        String msg = "未找到命令处理器: " + command;
        Serial.printf("[CommandDispatcher] %s\n", msg.c_str());
        return CommandResult::failure(msg);
    }

    // 获取已注册的命令列表
    String getRegisteredCommands() const {
        String result = "[";
        for (int i = 0; i < _count; i++) {
            if (_handlers[i]) {
                if (i > 0) result += ",";
                result += "\"" + _handlers[i]->getCommand() + "\"";
            }
        }
        result += "]";
        return result;
    }

    int getHandlerCount() const { return _count; }
};

/**
 * 命令自注册宏
 * 利用 C++ 静态初始化特性，在 main() 之前自动注册
 *
 * 用法:
 *   REGISTER_COMMAND(PlayAudioHandler);
 *   REGISTER_COMMAND(RecordAudioHandler);
 */
#define REGISTER_COMMAND(ClassName) \
    static bool _cmd_reg_##ClassName = []() { \
        static ClassName instance; \
        CommandDispatcher::getInstance().registerHandler(&instance); \
        return true; \
    }()

#endif // COMMAND_DISPATCHER_H
