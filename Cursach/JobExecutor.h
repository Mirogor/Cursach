#pragma once
#include "Task.h"
#include <memory>

class JobExecutor {
public:
    // Runs task synchronously (blocking). Returns exit code (or negative error).
    static int RunTask(const TaskPtr& task);
};
