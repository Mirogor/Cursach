#pragma once
#include <string>
#include <bitset>
#include <chrono>
#include <memory>

enum class TriggerType {
    ONCE = 0,
    INTERVAL = 1,
    DAILY = 2,
    WEEKLY = 3
};

struct Task {
    std::wstring id;
    std::wstring name;
    std::wstring description;
    std::wstring exePath;
    std::wstring arguments;
    std::wstring workingDirectory;
    bool enabled = true;
    TriggerType triggerType = TriggerType::DAILY;

    // Trigger params
    std::chrono::system_clock::time_point runOnceTime{};
    uint32_t intervalMinutes = 60;
    uint8_t dailyHour = 12, dailyMinute = 0, dailySecond = 0;
    std::bitset<7> weeklyDays;
    uint8_t weeklyHour = 12, weeklyMinute = 0, weeklySecond = 0;

    bool runIfMissed = true;

    // ← ДОБАВЛЕНО: Ограничение времени выполнения
    bool hasExecutionTimeout = false;      // Включен ли лимит
    uint32_t executionTimeoutMinutes = 5;  // Таймаут в минутах (по умолчанию 5)

    // Runtime info
    std::chrono::system_clock::time_point lastRunTime{};
    std::chrono::system_clock::time_point nextRunTime{};
    int lastExitCode = 0;
};
using TaskPtr = std::shared_ptr<Task>;