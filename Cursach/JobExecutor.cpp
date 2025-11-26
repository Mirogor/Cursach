#include "JobExecutor.h"
#include "Logger.h"
#include <Windows.h>
#include <string>

int JobExecutor::RunTask(const TaskPtr& task) {
    if (!task) return -1;
    g_Logger.Log(LogLevel::Info, L"JobExecutor", L"Starting task: " + task->name);
    std::wstring commandLine;
    if (!task->exePath.empty()) {
        commandLine = L"\"" + task->exePath + L"\"";
        if (!task->arguments.empty()) {
            commandLine += L" " + task->arguments;
        }
    }
    else {
        g_Logger.Log(LogLevel::Error, L"JobExecutor", L"No executable specified for task: " + task->name);
        return -1;
    }

    STARTUPINFOW si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);

    BOOL res = CreateProcessW(
        NULL,
        const_cast<LPWSTR>(commandLine.c_str()),
        NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL,
        task->workingDirectory.empty() ? NULL : task->workingDirectory.c_str(),
        &si, &pi
    );

    if (!res) {
        DWORD err = GetLastError();
        g_Logger.Log(LogLevel::Error, L"JobExecutor", L"CreateProcess failed (" + std::to_wstring(err) + L") for task: " + task->name);
        return -static_cast<int>(err);
    }

    DWORD waitRes = WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    if (!GetExitCodeProcess(pi.hProcess, &exitCode)) {
        g_Logger.Log(LogLevel::Warn, L"JobExecutor", L"GetExitCodeProcess failed for task: " + task->name);
    }
    else {
        g_Logger.Log(LogLevel::Info, L"JobExecutor", L"Task finished: " + task->name + L" code=" + std::to_wstring(exitCode));
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    task->lastExitCode = (int)exitCode;
    task->lastRunTime = std::chrono::system_clock::now();
    return (int)exitCode;
}
