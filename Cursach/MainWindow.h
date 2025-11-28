#pragma once
#include <Windows.h>
#include "TaskManager.h"
#include "Scheduler.h"

class MainWindow {
public:
    MainWindow(TaskManager* tm, Scheduler* sched);
    ~MainWindow();
    bool Create(HINSTANCE hInst);
    HWND Handle() const { return hwnd; }
private:
    HWND hwnd = nullptr;
    HWND hList = nullptr;
    TaskManager* taskManager;
    Scheduler* scheduler;
    void CreateControls();
    void RefreshList();
    void OnNew();
    void OnEdit();
    void OnDelete();
    void OnRun();
    void OnToggleEnabled();  // ← ДОБАВЛЕНО
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};