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
    // for ONCE: (используется полная дата-время)
    std::chrono::system_clock::time_point runOnceTime{};
    // for INTERVAL:
    uint32_t intervalMinutes = 60;
    // for DAILY:
    uint8_t dailyHour = 12, dailyMinute = 0, dailySecond = 0;
    // for WEEKLY:
    std::bitset<7> weeklyDays; // 0 = Sun, 1 = Mon, ..., 6 = Sat
    uint8_t weeklyHour = 12, weeklyMinute = 0, weeklySecond = 0;

    bool runIfMissed = true;

    // Runtime info
    std::chrono::system_clock::time_point lastRunTime{};
    std::chrono::system_clock::time_point nextRunTime{};
    int lastExitCode = 0;
};
using TaskPtr = std::shared_ptr<Task>;