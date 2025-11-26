#pragma once
#include <windows.h>
#include "Task.h"

class TaskDialog {
public:
    static bool ShowDialog(HWND parent, TaskPtr& task, bool isNew);
};
