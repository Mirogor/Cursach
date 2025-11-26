#pragma once
#include <memory>
#include "Task.h"
#include <windows.h>

class TaskDialog {
public:
    static bool ShowDialog(HWND parent, TaskPtr& task, bool isNew);
};
