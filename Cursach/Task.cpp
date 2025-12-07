// core/Task.cpp
#include "Task.h"
#include "Utils.h"
#include <sstream>

// Небольшие помощники для логирования / отладки.
// Эти функции не требуются для сборки, но полезны.

std::wstring TriggerTypeToWString(TriggerType t) {
    switch (t) {
    case TriggerType::ONCE: return L"ONCE";
    case TriggerType::INTERVAL: return L"INTERVAL";
    case TriggerType::DAILY: return L"DAILY";
    case TriggerType::WEEKLY: return L"WEEKLY";
    default: return L"UNKNOWN";
    }
}

std::wstring TaskToDebugString(const TaskPtr& task) {
    if (!task) return L"<null>";
    std::wstringstream ss;
    ss << L"Task[id=" << task->id
       << L", name=" << task->name
       << L", enabled=" << (task->enabled ? L"true" : L"false")
       << L", trigger=" << TriggerTypeToWString(task->triggerType)
       << L", exe=" << task->exePath
       << L"]";
    return ss.str();
}
