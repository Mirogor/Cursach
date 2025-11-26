#pragma once
#include <Windows.h>
#include "Task.h"
#include <memory>

class TaskDialog {
public:
    // modal dialog. If isNew==true, fills a new TaskPtr and returns it on OK.
    // If editing, pass existing TaskPtr.
    static bool ShowDialog(HWND parent, TaskPtr& task, bool isNew);
};
