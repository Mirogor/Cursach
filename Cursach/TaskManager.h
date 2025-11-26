#pragma once
#include "Task.h"
#include <vector>
#include <shared_mutex>
#include <functional>

class TaskManager {
public:
    TaskManager();
    ~TaskManager();

    std::vector<TaskPtr> GetAllTasks(); // copy of shared_ptrs
    void AddTask(const TaskPtr& task);
    void RemoveTask(const std::wstring& id);
    void UpdateTask(const TaskPtr& task);
    TaskPtr GetTaskById(const std::wstring& id);

    // Compute nextRunTime for a specific task (thread-safe call)
    void CalculateNextRun(const TaskPtr& task);

    // Save/load
    void Save();
    void Load();

    // Notification callback when tasks change (scheduler listens)
    using OnChangeFn = std::function<void()>;
    void SetOnChange(OnChangeFn fn);

private:
    std::vector<TaskPtr> tasks;
    mutable std::shared_mutex mutex;
    OnChangeFn onChange;
    class Persistence* persistence;
};
