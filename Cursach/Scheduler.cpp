#include "Scheduler.h"
#include "JobExecutor.h"
#include "Logger.h"
#include "Utils.h"
#include <chrono>

Scheduler::Scheduler(TaskManager* tm) : taskManager(tm) {}

Scheduler::~Scheduler() {
    Stop();
}

void Scheduler::Start() {
    if (running.load()) return;
    running.store(true);
    worker = std::thread(&Scheduler::ThreadProc, this);
    g_Logger.Log(LogLevel::Info, L"Scheduler", L"Scheduler started");
}

void Scheduler::Stop() {
    if (!running.load()) return;
    running.store(false);
    {
        std::lock_guard<std::mutex> lk(mtx);
        needWake = true;
    }
    cv.notify_one();
    if (worker.joinable()) worker.join();
    g_Logger.Log(LogLevel::Info, L"Scheduler", L"Scheduler stopped");
}

void Scheduler::Notify() {
    {
        std::lock_guard<std::mutex> lk(mtx);
        needWake = true;
    }
    cv.notify_one();
}

void Scheduler::ThreadProc() {
    using namespace std::chrono;
    while (running.load()) {
        auto tasks = taskManager->GetAllTasks();
        system_clock::time_point nextDeadline{};
        TaskPtr nextTask = nullptr;
        auto now = system_clock::now();

        for (auto& t : tasks) {
            if (!t->enabled) continue;
            if (t->nextRunTime.time_since_epoch().count() == 0) continue;

            if (t->nextRunTime <= now) {
                if (!nextTask || t->nextRunTime < nextTask->nextRunTime) {
                    nextTask = t;
                }
            }
            else {
                if (!nextTask) {
                    if (nextDeadline.time_since_epoch().count() == 0 || t->nextRunTime < nextDeadline) {
                        nextDeadline = t->nextRunTime;
                    }
                }
                else {
                    if (t->nextRunTime < nextDeadline) nextDeadline = t->nextRunTime;
                }
            }
        }

        if (nextTask) {
            g_Logger.Log(LogLevel::Info, L"Scheduler",
                L"Executing task: " + nextTask->name +
                L" | Type=" + std::to_wstring((int)nextTask->triggerType) +
                L" | hasTimeout=" + (nextTask->hasExecutionTimeout ? L"YES" : L"NO") +
                L" | timeoutMin=" + std::to_wstring(nextTask->executionTimeoutMinutes));

            TriggerType triggerType = nextTask->triggerType;

            // ← ИСПРАВЛЕНИЕ: Асинхронный запуск для INTERVAL, DAILY, WEEKLY
            if (triggerType == TriggerType::INTERVAL ||
                triggerType == TriggerType::DAILY ||
                triggerType == TriggerType::WEEKLY) {

                std::wstring typeStr = (triggerType == TriggerType::INTERVAL) ? L"INTERVAL" :
                    (triggerType == TriggerType::DAILY) ? L"DAILY" : L"WEEKLY";

                g_Logger.Log(LogLevel::Info, L"Scheduler",
                    L"⏱️ " + typeStr + L" task - launching asynchronously: " + nextTask->name);

                // Обновляем lastRunTime ДО запуска процесса
                nextTask->lastRunTime = system_clock::now();

                // Пересчитываем nextRunTime сразу
                taskManager->CalculateNextRun(nextTask);
                taskManager->Save();

                g_Logger.Log(LogLevel::Info, L"Scheduler",
                    L"✓ " + typeStr + L" task scheduled. Next run: " +
                    util::TimePointToWString(nextTask->nextRunTime));

                // Запускаем процесс в отдельном потоке (fire-and-forget)
                TaskPtr taskCopy = nextTask;
                std::thread([taskCopy, typeStr]() {
                    g_Logger.Log(LogLevel::Info, L"Scheduler",
                        L"🔄 " + typeStr + L" task background thread started: " + taskCopy->name);

                    int exitCode = JobExecutor::RunTask(taskCopy);

                    g_Logger.Log(LogLevel::Info, L"Scheduler",
                        L"✓ " + typeStr + L" task completed in background: " + taskCopy->name +
                        L" | exitCode=" + std::to_wstring(exitCode));
                    }).detach();

                // Продолжаем работу scheduler без ожидания завершения процесса
                continue;
            }

            // Для ONCE - синхронное выполнение (нужно дождаться завершения для отключения)
            if (triggerType == TriggerType::ONCE) {
                g_Logger.Log(LogLevel::Info, L"Scheduler",
                    L"🎯 ONCE task - executing synchronously: " + nextTask->name);

                int exitCode = JobExecutor::RunTask(nextTask);

                g_Logger.Log(LogLevel::Info, L"Scheduler",
                    L"Task completed: " + nextTask->name + L" | exitCode=" + std::to_wstring(exitCode));

                // ONCE всегда отключается после выполнения
                nextTask->enabled = false;
                nextTask->nextRunTime = {};

                if (exitCode == 999) {
                    g_Logger.Log(LogLevel::Warn, L"Scheduler",
                        L"Task '" + nextTask->name + L"' (ONCE) killed by timeout and disabled");
                }
                else {
                    g_Logger.Log(LogLevel::Info, L"Scheduler",
                        L"Task '" + nextTask->name + L"' (ONCE) completed and disabled");
                }

                taskManager->Save();
                continue;
            }
        }

        std::unique_lock<std::mutex> lk(mtx);
        if (nextDeadline.time_since_epoch().count() == 0) {
            cv.wait_for(lk, seconds(5), [&]() { return !running.load() || needWake; });
        }
        else {
            cv.wait_until(lk, nextDeadline, [&]() { return !running.load() || needWake; });
        }
        needWake = false;
    }
}