#include "Scheduler.h"
#include "JobExecutor.h"
#include "Logger.h"
#include <chrono>

Scheduler::Scheduler(TaskManager* tm) : taskManager(tm) {
    // taskManager should call Notify via OnChange callback
}

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
        // find next enabled task with earliest nextRunTime
        auto tasks = taskManager->GetAllTasks();
        system_clock::time_point nextDeadline{};
        TaskPtr nextTask = nullptr;
        auto now = system_clock::now();
        for (auto& t : tasks) {
            if (!t->enabled) continue;
            if (t->nextRunTime.time_since_epoch().count() == 0) continue;
            if (t->nextRunTime <= now) {
                // ready to run immediately; choose the earliest
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
            // Execute it synchronously in this thread (could offload to thread pool)
            g_Logger.Log(LogLevel::Info, L"Scheduler", L"Executing task: " + nextTask->name);
            int code = JobExecutor::RunTask(nextTask);
            (void)code;
            // Recalculate next run
            taskManager->CalculateNextRun(nextTask);
            taskManager->Save();
            // notify UI via TaskManager.onChange already triggered in Save if desired
            continue; // look for next task immediately
        }

        // wait until nextDeadline or until notified
        std::unique_lock<std::mutex> lk(mtx);
        if (nextDeadline.time_since_epoch().count() == 0) {
            // no upcoming task; wait for a notification
            cv.wait_for(lk, seconds(5), [&]() { return !running.load() || needWake; });
        }
        else {
            cv.wait_until(lk, nextDeadline, [&]() { return !running.load() || needWake; });
        }
        needWake = false;
    }
}
