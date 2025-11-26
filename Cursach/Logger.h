#pragma once
#include <string>
#include <mutex>

/// Logger.h
/// »справлЄн enum LogLevel Ч значени€ не конфликтуют с WinAPI макросами.
enum class LogLevel { Debug, Info, Warn, Error };

class Logger {
public:
    Logger();
    ~Logger();

    // thread-safe logging
    void Log(LogLevel level, const std::wstring& tag, const std::wstring& message);

private:
    std::wstring logFilePath_;
    std::mutex mtx_;
};

// ќбщий экземпл€р (если у вас используетс€ глобально)
extern Logger g_Logger;
