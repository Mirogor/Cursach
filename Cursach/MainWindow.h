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
    HWND hCheckSortName = nullptr;
    HWND hCheckSortStatus = nullptr;
    HWND hStatLabel = nullptr;  // ← ДОБАВЛЕНО: метка статистики
    TaskManager* taskManager;
    Scheduler* scheduler;

    bool sortByName = false;
    bool sortByStatus = false;

    void CreateControls();
    void RefreshList();
    void UpdateStatistics();  // ← ДОБАВЛЕНО
    void OnNew();
    void OnEdit();
    void OnDelete();
    void OnRun();
    void OnToggleEnabled();
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};