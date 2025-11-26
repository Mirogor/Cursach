#pragma once
#include "TaskManager.h"
#include <thread>
#include <atomic>
#include <condition_variable>

class Scheduler {
public:
    Scheduler(TaskManager* tm);
    ~Scheduler();
    void Start();
    void Stop();
    // notify scheduler that tasks changed (recalculate next)
    void Notify();
private:
    void ThreadProc();
    TaskManager* taskManager;
    std::thread worker;
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic<bool> running{ false };
    bool needWake = false;
};
