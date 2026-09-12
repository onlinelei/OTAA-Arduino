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
    bool isAsync;  // true = handler 自己负责 ack（异步执行），dispatcher 不自动 ack

    static CommandResult success(const String& result = "") {
        return {true, result, "", false};
    }

    static CommandResult failure(const String& errorMsg) {
        return {false, "", errorMsg, false};
    }

    // 异步：handler 已启动后台任务，会自行调用 ackCommand
    static CommandResult asyncStarted() {
        return {true, "{\"status\":\"async_started\"}", "", true};
    }
};

// 命令处理器接口
class CommandHandler {
public:
    virtual ~CommandHandler() {}
    virtual String getCommand() const = 0;
    virtual CommandResult execute(int commandId, const String& params) = 0;

    /**
     * 有**新命令**到达、而本 handler 正在异步执行时调用，给 handler 一个
     * "让位"的机会。
     *
     * 背景：平台侧 `replace` 策略的本意是"新命令取代旧的"。但 `play_audio`
     * 这类命令是**异步**的（execute() 立刻返回 asyncStarted()，由后台任务在
     * 播放结束时才 ack），命令会在平台侧长时间停在 status=1。旧实现里
     * 「执行中就不再轮询」，于是播放中下发的命令**根本送不到设备**，只能等
     * 整首播完——用户观感就是"下发的命令很久才播放"。
     *
     * ⚠️ 实现者应当**只对同类新命令让位**（`newCommand == getCommand()`）。
     *    别的命令来了不该把当前工作无谓地掐掉，让它排进 1 深槽等当前命令
     *    结束即可。例：正在播音乐时来一条 `stock_config`，不该停歌。
     *
     * @param newCommand 新到达命令的 command code（如 "play_audio"）
     * @return true  = 已停止当前工作、可以立刻接管（框架马上 dispatch 新命令）
     *         false = 无法让位（新命令会暂存，等当前命令结束后再执行）
     */
    virtual bool preempt(const String& newCommand) { (void)newCommand; return false; }

    /**
     * 异步命令执行期间的"存活探询"，供 OTAA 的命令看门狗做**续期**用。
     *
     * 没有它的话：`play_audio` 播 300s 恰好会与默认 300s 的 `_commandTimeout`
     * 打平，看门狗抢在真实结果之前发出假的 `status=3 "Timeout"` ack，随后真正的
     * 成功 ack 又被平台的幂等保护拒绝（只接受 status==1）——实测一次**完美的
     * 300 秒播放被记录成失败**。
     *
     * @return true = 确实还在正常工作（看门狗应续期，不要判超时）
     */
    virtual bool isAlive() { return false; }
};

// 命令分发器（单例）
class CommandDispatcher {
private:
    static const int MAX_HANDLERS = 32;

    CommandHandler* _handlers[MAX_HANDLERS];
    int _count;
    void* _userData;

    // 当前正在**异步**执行的 handler（execute() 返回了 asyncStarted 的那个）。
    // 用于 preempt() 让位与看门狗续期，见 CommandHandler 的两个虚函数。
    CommandHandler* _runningHandler;
    int _runningCommandId;

    CommandDispatcher() : _count(0), _userData(nullptr),
                          _runningHandler(nullptr), _runningCommandId(0) {
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
        // 去重：同一 command code 只注册一次
        String cmd = handler->getCommand();
        for (int i = 0; i < _count; i++) {
            if (_handlers[i] && _handlers[i]->getCommand() == cmd) {
                return false;  // 已注册，跳过
            }
        }
        _handlers[_count++] = handler;
        return true;
    }

    CommandResult dispatch(int commandId, const String& command, const String& params) {
        for (int i = 0; i < _count; i++) {
            if (_handlers[i] && _handlers[i]->getCommand() == command) {
                CommandResult r = _handlers[i]->execute(commandId, params);
                // 记下异步执行的 handler，供 preempt() / isAlive() 使用
                if (r.isAsync) {
                    _runningHandler = _handlers[i];
                    _runningCommandId = commandId;
                }
                return r;
            }
        }
        return CommandResult::failure(String("Handler not found: ") + command.c_str());
    }

    /** 该命令是否有对应的 handler（用于判断能否接管） */
    bool hasHandler(const String& command) const {
        for (int i = 0; i < _count; i++) {
            if (_handlers[i] && _handlers[i]->getCommand() == command) return true;
        }
        return false;
    }

    /**
     * 请当前异步执行的 handler 让位给新命令。
     * @return true = 已让出（框架可立刻 dispatch 新命令）
     */
    bool preemptRunning(const String& newCommand) {
        if (_runningHandler == nullptr) return false;
        bool ok = _runningHandler->preempt(newCommand);
        if (ok) {
            _runningHandler = nullptr;
            _runningCommandId = 0;
        }
        return ok;
    }

    /** 异步命令结束（ack 完成）时清掉记录 */
    void clearRunning(int commandId) {
        if (_runningCommandId == commandId) {
            _runningHandler = nullptr;
            _runningCommandId = 0;
        }
    }

    /** 当前异步命令的 handler 是否仍存活（看门狗续期用） */
    bool runningHandlerAlive() const {
        return _runningHandler != nullptr && _runningHandler->isAlive();
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

// 命令自注册宏（registerHandler 内部去重，安全多 TU 包含）
#define REGISTER_COMMAND(ClassName) \
    static bool _cmd_reg_##ClassName = []() { \
        static ClassName instance; \
        CommandDispatcher::getInstance().registerHandler(&instance); \
        return true; \
    }()

#endif // COMMAND_DISPATCHER_H
