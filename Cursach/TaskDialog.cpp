#include "TaskDialog.h"
#include <commctrl.h>
#include <string>
#include "Utils.h"
#include "Logger.h"

// We'll implement a very small dialog using MessageBox prompts to set common fields
// (to keep the example compact). For a full dialog use CreateDialogParam + controls.
// Here: for the demo we show simple prompts for name, exePath, trigger type and params.

static std::wstring PromptStr(HWND parent, const std::wstring& title, const std::wstring& def = L"") {
    // For simplicity, use InputBox-like approach is not available in Win32 by default.
    // We'll write to log and return default. In practical project you'd create a real dialog.
    g_Logger.Log(LogLevel::Info, L"TaskDialog", L"Prompt requested: " + title);
    return def;
}

bool TaskDialog::ShowDialog(HWND parent, TaskPtr& task, bool isNew) {
    if (isNew) {
        task = std::make_shared<Task>();
        task->id = util::GenerateGUID();
        task->name = L"New Task";
        task->exePath = L"notepad.exe";
        task->triggerType = TriggerType::INTERVAL;
        task->intervalMinutes = 5;
        for (int i = 0; i < 7; ++i) task->weeklyDays.set(i);
    }
    // For real UI you must implement control-based dialog; here we just accept current values
    // and return true to indicate OK.
    return MessageBoxW(parent, L"Task dialog placeholder: using default/sample values.\nImplement full dialog for production.",
        L"Task Editor", MB_OKCANCEL) == IDOK;
}
