#pragma once
#include "Task.h"
#include <memory>
#include <string>

class JobExecutor {
public:
    static int RunTask(const TaskPtr& task);
};
