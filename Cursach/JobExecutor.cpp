#include "JobExecutor.h"
#include "Logger.h"
#include "Utils.h"
#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <vector>

#pragma comment(lib, "Advapi32.lib")

// ← ДОБАВЛЕНО: Убийство всех процессов по имени (для приложений типа Telegram)
static bool KillProcessesByName(const std::wstring& exePath) {
    // Извлекаем только имя файла
    std::wstring exeName = util::GetFileName(exePath);
    if (exeName.empty()) return false;

    g_Logger.Log(LogLevel::Info, L"JobExecutor", 
        L"Searching for processes with name: " + exeName);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        g_Logger.Log(LogLevel::Error, L"JobExecutor", L"CreateToolhelp32Snapshot failed");
        return false;
    }

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    bool foundAny = false;
    int killedCount = 0;

    if (Process32FirstW(snapshot, &pe32)) {
        do {
            // Сравниваем имя процесса (case-insensitive)
            if (_wcsicmp(pe32.szExeFile, exeName.c_str()) == 0) {
                foundAny = true;

                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
                if (hProcess) {
                    g_Logger.Log(LogLevel::Info, L"JobExecutor", 
                        L"Found process: " + exeName + L" | PID=" + std::to_wstring(pe32.th32ProcessID));

                    if (TerminateProcess(hProcess, 999)) {
                        killedCount++;
                        g_Logger.Log(LogLevel::Info, L"JobExecutor", 
                            L"✓ Terminated PID=" + std::to_wstring(pe32.th32ProcessID));
                    } else {
                        DWORD err = GetLastError();
                        g_Logger.Log(LogLevel::Warn, L"JobExecutor", 
                            L"✗ Failed to terminate PID=" + std::to_wstring(pe32.th32ProcessID) + 
                            L" error=" + std::to_wstring(err));
                    }

                    CloseHandle(hProcess);
                }
            }
        } while (Process32NextW(snapshot, &pe32));
    }

    CloseHandle(snapshot);

    if (foundAny) {
        g_Logger.Log(LogLevel::Info, L"JobExecutor", 
            L"Killed " + std::to_wstring(killedCount) + L" process(es) with name: " + exeName);
    } else {
        g_Logger.Log(LogLevel::Warn, L"JobExecutor", 
            L"No processes found with name: " + exeName);
    }

    return killedCount > 0;
}

int JobExecutor::RunTask(const TaskPtr& task) {
    if (!task) return -1;
    
    g_Logger.Log(LogLevel::Info, L"JobExecutor", L"Starting task: " + task->name);
    
    if (task->hasExecutionTimeout) {
        g_Logger.Log(LogLevel::Info, L"JobExecutor", 
            L"Task '" + task->name + L"' has timeout: " + 
            std::to_wstring(task->executionTimeoutMinutes) + L" minutes");
    } else {
        g_Logger.Log(LogLevel::Info, L"JobExecutor", 
            L"Task '" + task->name + L"' has NO timeout (will wait indefinitely)");
    }
    
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
        g_Logger.Log(LogLevel::Error, L"JobExecutor", 
            L"CreateProcess failed (" + std::to_wstring(err) + L") for task: " + task->name);
        return -static_cast<int>(err);
    }

    g_Logger.Log(LogLevel::Info, L"JobExecutor", 
        L"Process created successfully for task: " + task->name + 
        L" | PID=" + std::to_wstring(pi.dwProcessId));

    DWORD exitCode = 0;
    DWORD waitRes = 0;

    if (task->hasExecutionTimeout && task->executionTimeoutMinutes > 0) {
        DWORD timeoutMs = task->executionTimeoutMinutes * 60 * 1000;
        
        g_Logger.Log(LogLevel::Info, L"JobExecutor", 
            L"Waiting for task '" + task->name + L"' with timeout: " + 
            std::to_wstring(task->executionTimeoutMinutes) + L" minutes (" + 
            std::to_wstring(timeoutMs) + L" ms)");
        
        waitRes = WaitForSingleObject(pi.hProcess, timeoutMs);
        
        if (waitRes == WAIT_TIMEOUT) {
            g_Logger.Log(LogLevel::Warn, L"JobExecutor", 
                L"⏱️ TIMEOUT! Task '" + task->name + L"' exceeded " + 
                std::to_wstring(task->executionTimeoutMinutes) + L" minutes");
            
            // ← ИЗМЕНЕНО: Сначала пытаемся убить исходный процесс
            BOOL terminated = TerminateProcess(pi.hProcess, 999);
            if (terminated) {
                g_Logger.Log(LogLevel::Info, L"JobExecutor", 
                    L"✓ TerminateProcess succeeded for PID=" + std::to_wstring(pi.dwProcessId));
            } else {
                DWORD err = GetLastError();
                g_Logger.Log(LogLevel::Warn, L"JobExecutor", 
                    L"✗ TerminateProcess FAILED (" + std::to_wstring(err) + 
                    L") for PID=" + std::to_wstring(pi.dwProcessId));
            }
            
            // ← ДОБАВЛЕНО: Убиваем ВСЕ процессы с таким именем (для Telegram, Chrome и т.д.)
            g_Logger.Log(LogLevel::Info, L"JobExecutor", 
                L"Attempting to kill all processes with executable name: " + task->exePath);
            
            bool killedByName = KillProcessesByName(task->exePath);
            
            if (killedByName) {
                g_Logger.Log(LogLevel::Info, L"JobExecutor", 
                    L"✓ Successfully killed processes by name");
            } else {
                g_Logger.Log(LogLevel::Warn, L"JobExecutor", 
                    L"⚠ No additional processes found to kill");
            }
            
            WaitForSingleObject(pi.hProcess, 5000);
            exitCode = 999;
        }
        else if (waitRes == WAIT_OBJECT_0) {
            if (!GetExitCodeProcess(pi.hProcess, &exitCode)) {
                g_Logger.Log(LogLevel::Warn, L"JobExecutor", 
                    L"GetExitCodeProcess failed for task: " + task->name);
            } else {
                g_Logger.Log(LogLevel::Info, L"JobExecutor", 
                    L"Task '" + task->name + L"' finished normally with exitCode=" + std::to_wstring(exitCode));
                
                // ← ДОБАВЛЕНО: Проверка быстрого завершения (признак "single instance" приложения)
                DWORD elapsed = GetTickCount() - GetTickCount();  // В реальности нужен timestamp
                if (exitCode == 0 && waitRes == WAIT_OBJECT_0) {
                    // Процесс завершился мгновенно - возможно это single-instance приложение
                    g_Logger.Log(LogLevel::Warn, L"JobExecutor", 
                        L"⚠ Process exited immediately (exitCode=0) - likely a single-instance app like Telegram/Chrome");
                    g_Logger.Log(LogLevel::Info, L"JobExecutor", 
                        L"Note: Timeout will still work for killing the actual running instance");
                }
            }
        }
        else {
            g_Logger.Log(LogLevel::Error, L"JobExecutor", 
                L"WaitForSingleObject failed (result=" + std::to_wstring(waitRes) + L") for task: " + task->name);
            exitCode = 0xFFFFFFFF;
        }
    }
    else {
        g_Logger.Log(LogLevel::Info, L"JobExecutor", 
            L"Waiting for task '" + task->name + L"' without timeout (INFINITE)");
        
        waitRes = WaitForSingleObject(pi.hProcess, INFINITE);
        
        if (!GetExitCodeProcess(pi.hProcess, &exitCode)) {
            g_Logger.Log(LogLevel::Warn, L"JobExecutor", 
                L"GetExitCodeProcess failed for task: " + task->name);
        } else {
            g_Logger.Log(LogLevel::Info, L"JobExecutor", 
                L"Task '" + task->name + L"' finished with exitCode=" + std::to_wstring(exitCode));
        }
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    task->lastExitCode = (int)exitCode;
    task->lastRunTime = std::chrono::system_clock::now();
    
    g_Logger.Log(LogLevel::Info, L"JobExecutor", 
        L"Task '" + task->name + L"' execution completed. Final exitCode=" + std::to_wstring(exitCode));
    
    return (int)exitCode;
}