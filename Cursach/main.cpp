#include <Windows.h>
#include <commctrl.h>
#include "TaskManager.h"
#include "Scheduler.h"
#include "MainWindow.h"
#include "Logger.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    CoInitialize(NULL);
    InitCommonControls();

    g_Logger.Log(LogLevel::Info, L"Main", L"Starting MiniTaskScheduler");

    TaskManager tm;
    Scheduler sched(&tm);
    tm.SetOnChange([&sched]() { sched.Notify(); });

    MainWindow mainWin(&tm, &sched);
    if (!mainWin.Create(hInstance)) {
        MessageBoxW(NULL, L"Failed to create main window", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // start scheduler
    sched.Start();

    // message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    sched.Stop();
    tm.Save();

    g_Logger.Log(LogLevel::Info, L"Main", L"Exiting MiniTaskScheduler");
    CoUninitialize();
    return 0;
}
