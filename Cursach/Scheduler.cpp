#include "Scheduler.h"
#include "JobExecutor.h"
#include "Logger.h"
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
            // ← КРИТИЧНО: Логируем ПЕРЕД выполнением для дебага
            g_Logger.Log(LogLevel::Info, L"Scheduler",
                L"Executing task: " + nextTask->name +
                L" | hasTimeout=" + (nextTask->hasExecutionTimeout ? L"YES" : L"NO") +
                L" | timeoutMin=" + std::to_wstring(nextTask->executionTimeoutMinutes));

            TriggerType triggerType = nextTask->triggerType;

            // ← ИСПРАВЛЕНО: JobExecutor работает с той же shared_ptr копией
            int exitCode = JobExecutor::RunTask(nextTask);

            g_Logger.Log(LogLevel::Info, L"Scheduler",
                L"Task completed: " + nextTask->name + L" | exitCode=" + std::to_wstring(exitCode));

            if (triggerType == TriggerType::ONCE) {
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
            }
            else {
                if (exitCode == 999) {
                    g_Logger.Log(LogLevel::Warn, L"Scheduler",
                        L"Task '" + nextTask->name + L"' killed by timeout (will run again next cycle)");
                }
            }

            taskManager->CalculateNextRun(nextTask);
            taskManager->Save();
            continue;
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