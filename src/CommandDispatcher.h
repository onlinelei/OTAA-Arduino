/**
 * CommandDispatcher - 自定义命令分发器（平台无关）
 *
 * 提供命令自注册机制，设备端开发者只需:
 * 1. 实现 CommandHandler 接口
 * 2. 使用 REGISTER_COMMAND 宏注册
 * 3. OTAA 库自动分发命令到对应 handler
 */

#ifndef COMMAND_DISPATCHER_H
#define COMMAND_DISPATCHER_H

#include "Hal.h"  // 提供 String 类型定义

// 前向声明
class OTAA;

// 命令执行结果
struct CommandResult {
    bool isSuccess;
    String result;
    String errorMsg;

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
    virtual String getCommand() const = 0;
    virtual CommandResult execute(int commandId, const String& params) = 0;
};

// 命令分发器（单例）
class CommandDispatcher {
private:
    static const int MAX_HANDLERS = 32;

    CommandHandler* _handlers[MAX_HANDLERS];
    int _count;
    void* _userData;

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

    void setUserData(void* data) { _userData = data; }
    void* getUserData() const { return _userData; }

    OTAA* getOTAAPtr() {
        return static_cast<OTAA*>(_userData);
    }

    bool registerHandler(CommandHandler* handler) {
        if (_count >= MAX_HANDLERS || !handler) return false;
        _handlers[_count++] = handler;
        return true;
    }

    CommandResult dispatch(int commandId, const String& command, const String& params) {
        for (int i = 0; i < _count; i++) {
            if (_handlers[i] && _handlers[i]->getCommand() == command) {
                return _handlers[i]->execute(commandId, params);
            }
        }
        return CommandResult::failure(String("Handler not found: ") + command.c_str());
    }

    String getRegisteredCommands() const {
        String result = "[";
        for (int i = 0; i < _count; i++) {
            if (_handlers[i]) {
                if (i > 0) result += ",";
                result += String("\"") + _handlers[i]->getCommand().c_str() + "\"";
            }
        }
        result += "]";
        return result;
    }

    int getHandlerCount() const { return _count; }
};

// 命令自注册宏
#define REGISTER_COMMAND(ClassName) \
    static bool _cmd_reg_##ClassName = []() { \
        static ClassName instance; \
        CommandDispatcher::getInstance().registerHandler(&instance); \
        return true; \
    }()

#endif // COMMAND_DISPATCHER_H
