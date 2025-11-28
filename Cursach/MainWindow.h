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
    HWND hCheckSortName = nullptr;      // ← ДОБАВЛЕНО: Checkbox для сортировки по имени
    HWND hCheckSortStatus = nullptr;    // ← ДОБАВЛЕНО: Checkbox для сортировки по статусу
    TaskManager* taskManager;
    Scheduler* scheduler;

    // Флаги сортировки
    bool sortByName = false;            // ← ДОБАВЛЕНО
    bool sortByStatus = false;          // ← ДОБАВЛЕНО

    void CreateControls();
    void RefreshList();
    void OnNew();
    void OnEdit();
    void OnDelete();
    void OnRun();
    void OnToggleEnabled();
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};