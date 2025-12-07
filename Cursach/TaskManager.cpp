#include "TaskManager.h"
#include "Persistence.h"
#include "Logger.h"
#include "Utils.h"

#include <algorithm>
#include <chrono>
#include <shared_mutex>
#include <string>

TaskManager::TaskManager() {
    persistence = new Persistence();
    Load();
}

TaskManager::~TaskManager() {
    Save();
    delete persistence;
}

std::vector<TaskPtr> TaskManager::GetAllTasks() {
    std::shared_lock lock(mutex);
    return tasks;
}

void TaskManager::AddTask(const TaskPtr& task) {
    std::unique_lock lock(mutex);
    tasks.push_back(task);
    CalculateNextRun(task);
    lock.unlock();

    Save();
    if (onChange) onChange();

    g_Logger.Log(
        LogLevel::Info,
        L"TaskManager",
        std::wstring(L"Added task: ") + task->name
    );
}

void TaskManager::RemoveTask(const std::wstring& id) {
    std::unique_lock<std::shared_mutex> lock(mutex);

    auto it = std::find_if(tasks.begin(), tasks.end(),
        [&](const TaskPtr& t) { return t && t->id == id; });

    if (it == tasks.end()) {
        g_Logger.Log(LogLevel::Info, L"TaskManager", L"RemoveTask: not found id=" + id);
        return;
    }

    std::wstring name = (*it)->name;
    tasks.erase(it);
    lock.unlock();

    Save();
    if (onChange) onChange();

    g_Logger.Log(LogLevel::Info, L"TaskManager", L"Removed task: " + name);
}

void TaskManager::UpdateTask(const TaskPtr& task) {
    std::unique_lock lock(mutex);
    for (auto& t : tasks) {
        if (t->id == task->id) {
            t = task;
            CalculateNextRun(t);
            break;
        }
    }
    lock.unlock();

    Save();
    if (onChange) onChange();

    g_Logger.Log(
        LogLevel::Info,
        L"TaskManager",
        std::wstring(L"Updated task: ") + task->name
    );
}

TaskPtr TaskManager::GetTaskById(const std::wstring& id) {
    std::shared_lock lock(mutex);
    for (auto& t : tasks)
        if (t->id == id)
            return t;
    return nullptr;
}

void TaskManager::CalculateNextRun(const TaskPtr& task) {
    if (!task) return;

    using namespace std::chrono;

    auto now = system_clock::now();
    switch (task->triggerType) {

    case TriggerType::ONCE:
        if (task->runOnceTime.time_since_epoch().count() == 0) {
            task->nextRunTime = {};
        }
        else {
            task->nextRunTime = task->runOnceTime;

            if (task->nextRunTime <= now) {
                if (task->runIfMissed) {
                    task->nextRunTime = now;
                }
                else {
                    task->nextRunTime = {};
                }
            }
        }
        break;

    case TriggerType::INTERVAL:
        if (task->lastRunTime.time_since_epoch().count() == 0)
            task->nextRunTime = now + minutes(task->intervalMinutes);
        else
            task->nextRunTime = task->lastRunTime + minutes(task->intervalMinutes);
        break;

    case TriggerType::DAILY: {
        time_t tt = system_clock::to_time_t(now);
        tm local{};
        localtime_s(&local, &tt);

        local.tm_hour = task->dailyHour;
        local.tm_min = task->dailyMinute;
        local.tm_sec = task->dailySecond;

        time_t target = mktime(&local);
        auto tp = system_clock::from_time_t(target);

        if (tp <= now) tp += hours(24);

        task->nextRunTime = tp;
        break;
    }

    case TriggerType::WEEKLY: {
        time_t tt = system_clock::to_time_t(now);
        tm local{};
        localtime_s(&local, &tt);

        int today = local.tm_wday;  // 0=Sunday, 1=Monday, ..., 6=Saturday
        bool found = false;

        g_Logger.Log(LogLevel::Debug, L"TaskManager",
            L"WEEKLY: Calculating next run for task: " + task->name +
            L" | Today: " + std::to_wstring(today) +
            L" | Target time: " + std::to_wstring(task->weeklyHour) + L":" +
            std::to_wstring(task->weeklyMinute));

        // Проверяем следующие 7 дней (эта неделя)
        for (int offset = 0; offset < 7; ++offset) {
            int d = (today + offset) % 7;

            // Пропускаем дни, которые не выбраны
            if (!task->weeklyDays.test(d)) {
                continue;
            }

            // Строим целевое время для этого дня
            tm cand = local;
            cand.tm_mday += offset;
            cand.tm_hour = task->weeklyHour;
            cand.tm_min = task->weeklyMinute;
            cand.tm_sec = task->weeklySecond;

            time_t ct = mktime(&cand);  // mktime нормализует дату
            auto tp = system_clock::from_time_t(ct);

            g_Logger.Log(LogLevel::Debug, L"TaskManager",
                L"  Checking offset=" + std::to_wstring(offset) +
                L" | day=" + std::to_wstring(d) +
                L" | time in future: " + (tp > now ? L"YES" : L"NO"));

            // Берем только если время строго в будущем
            if (tp > now) {
                task->nextRunTime = tp;
                found = true;
                g_Logger.Log(LogLevel::Info, L"TaskManager",
                    L"✓ WEEKLY: Found next run in THIS week: " +
                    util::TimePointToWString(tp));
                break;
            }
        }

        // Если не нашли на этой неделе, ищем на следующей
        if (!found) {
            g_Logger.Log(LogLevel::Debug, L"TaskManager",
                L"WEEKLY: No suitable time this week, checking NEXT week");

            for (int offset = 7; offset < 14; ++offset) {
                int d = (today + offset) % 7;

                if (!task->weeklyDays.test(d)) {
                    continue;
                }

                tm cand = local;
                cand.tm_mday += offset;
                cand.tm_hour = task->weeklyHour;
                cand.tm_min = task->weeklyMinute;
                cand.tm_sec = task->weeklySecond;

                time_t ct = mktime(&cand);
                auto tp = system_clock::from_time_t(ct);

                task->nextRunTime = tp;
                found = true;
                g_Logger.Log(LogLevel::Info, L"TaskManager",
                    L"✓ WEEKLY: Found next run in NEXT week: " +
                    util::TimePointToWString(tp));
                break;
            }
        }

        // Если все еще не нашли - ошибка конфигурации (не выбран ни один день)
        if (!found) {
            task->nextRunTime = {};
            g_Logger.Log(LogLevel::Warn, L"TaskManager",
                L"⚠ WEEKLY: No days selected for task: " + task->name);
        }

        break;
    }

    default:
        task->nextRunTime = {};
    }
}

void TaskManager::Save() {
    std::shared_lock lock(mutex);
    persistence->Save(tasks);
}

void TaskManager::Load() {
    auto loaded = persistence->Load();

    {
        std::unique_lock lock(mutex);
        tasks = loaded;

        for (auto& t : tasks) {
            if (t->id.empty())
                t->id = util::GenerateGUID();
            CalculateNextRun(t);
        }
    }

    if (onChange) onChange();
}

void TaskManager::SetOnChange(OnChangeFn fn) {
    onChange = fn;
}