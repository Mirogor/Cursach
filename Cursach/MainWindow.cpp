#include "MainWindow.h"
#include <commctrl.h>
#include "TaskDialog.h"
#include "Utils.h"
#include "Logger.h"
#include <string>
#include "JobExecutor.h"

MainWindow::MainWindow(TaskManager* tm, Scheduler* sched) : taskManager(tm), scheduler(sched) {}

MainWindow::~MainWindow() {}

bool MainWindow::Create(HINSTANCE hInst) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = MainWindow::WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"MiniTaskSchedulerClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    hwnd = CreateWindowW(wc.lpszClassName, L"Mini Task Scheduler",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 900, 500,
        NULL, NULL, hInst, this);
    if (!hwnd) return false;
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    return true;
}

void MainWindow::CreateControls() {
    RECT rc;
    GetClientRect(hwnd, &rc);
    hList = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
        10, 50, rc.right - 20, rc.bottom - 60, hwnd, (HMENU)1001, GetModuleHandle(NULL), NULL);

    ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    LVCOLUMNW col{};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.pszText = (LPWSTR)L"Name"; col.cx = 220; ListView_InsertColumn(hList, 0, &col);
    col.pszText = (LPWSTR)L"Status"; col.cx = 80; ListView_InsertColumn(hList, 1, &col);
    col.pszText = (LPWSTR)L"Trigger"; col.cx = 120; ListView_InsertColumn(hList, 2, &col);
    col.pszText = (LPWSTR)L"Next Run"; col.cx = 200; ListView_InsertColumn(hList, 3, &col);
    col.pszText = (LPWSTR)L"Command"; col.cx = 260; ListView_InsertColumn(hList, 4, &col);

    CreateWindowW(L"BUTTON", L"New", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 10, 80, 30, hwnd, (HMENU)2001, GetModuleHandle(NULL), NULL);
    CreateWindowW(L"BUTTON", L"Edit", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 100, 10, 80, 30, hwnd, (HMENU)2002, GetModuleHandle(NULL), NULL);
    CreateWindowW(L"BUTTON", L"Delete", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 190, 10, 80, 30, hwnd, (HMENU)2003, GetModuleHandle(NULL), NULL);
    CreateWindowW(L"BUTTON", L"Run", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 280, 10, 80, 30, hwnd, (HMENU)2004, GetModuleHandle(NULL), NULL);
    CreateWindowW(L"BUTTON", L"Enable/Disable", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 370, 10, 120, 30, hwnd, (HMENU)2006, GetModuleHandle(NULL), NULL);  // ? ДОБАВЛЕНО
    CreateWindowW(L"BUTTON", L"Refresh", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 500, 10, 80, 30, hwnd, (HMENU)2005, GetModuleHandle(NULL), NULL);

    RefreshList();
}

void MainWindow::RefreshList() {
    if (!hList) return;
    ListView_DeleteAllItems(hList);
    auto tasks = taskManager->GetAllTasks();
    int idx = 0;
    const wchar_t* triggerNames[] = { L"Once", L"Interval", L"Daily", L"Weekly" };
    for (auto& t : tasks) {
        LVITEMW it{};
        it.mask = LVIF_TEXT;
        it.iItem = idx;
        it.pszText = const_cast<LPWSTR>(t->name.c_str());
        ListView_InsertItem(hList, &it);
        ListView_SetItemText(hList, idx, 1, const_cast<LPWSTR>((t->enabled ? L"Enabled" : L"Disabled")));
        ListView_SetItemText(hList, idx, 2, const_cast<LPWSTR>(triggerNames[(int)t->triggerType]));
        auto ns = util::TimePointToWString(t->nextRunTime);

        ListView_SetItemText(hList, idx, 3, const_cast<LPWSTR>(ns.c_str()));
        ListView_SetItemText(hList, idx, 4, const_cast<LPWSTR>(t->exePath.c_str()));
        ++idx;
    }
}

void MainWindow::OnNew() {
    TaskPtr t;
    if (TaskDialog::ShowDialog(hwnd, t, true)) {
        taskManager->AddTask(t);
        scheduler->Notify();
        RefreshList();
    }
}

void MainWindow::OnEdit() {
    int sel = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
    if (sel < 0) return;
    auto tasks = taskManager->GetAllTasks();
    if (sel >= (int)tasks.size()) return;
    TaskPtr t = tasks[sel];
    if (TaskDialog::ShowDialog(hwnd, t, false)) {
        taskManager->UpdateTask(t);
        scheduler->Notify();
        RefreshList();
    }
}

void MainWindow::OnDelete() {
    int sel = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
    if (sel < 0) return;
    auto tasks = taskManager->GetAllTasks();
    if (sel >= (int)tasks.size()) return;
    TaskPtr t = tasks[sel];
    if (MessageBoxW(hwnd, (L"Delete task: " + t->name).c_str(), L"Confirm", MB_YESNO) == IDYES) {
        taskManager->RemoveTask(t->id);
        scheduler->Notify();
        RefreshList();
    }
}

void MainWindow::OnRun() {
    int sel = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
    if (sel < 0) return;
    auto tasks = taskManager->GetAllTasks();
    if (sel >= (int)tasks.size()) return;
    TaskPtr t = tasks[sel];
    // Run in separate thread to avoid blocking UI
    std::thread([t, this]() {
        JobExecutor::RunTask(t);
        taskManager->CalculateNextRun(t);
        taskManager->Save();
        // inform UI on main thread
        PostMessageW(this->hwnd, WM_USER + 100, 0, 0);
        }).detach();
}

// ? ДОБАВЛЕНО: Новая функция переключения статуса
void MainWindow::OnToggleEnabled() {
    int sel = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
    if (sel < 0) {
        MessageBoxW(hwnd, L"Please select a task first.", L"Info", MB_OK | MB_ICONINFORMATION);
        return;
    }

    auto tasks = taskManager->GetAllTasks();
    if (sel >= (int)tasks.size()) return;

    TaskPtr t = tasks[sel];

    // Переключаем статус
    t->enabled = !t->enabled;

    // Логируем изменение
    g_Logger.Log(
        LogLevel::Info,
        L"MainWindow",
        L"Task '" + t->name + L"' " + (t->enabled ? L"enabled" : L"disabled")
    );

    // Если задача включена, пересчитываем nextRunTime
    if (t->enabled) {
        taskManager->CalculateNextRun(t);
    }
    else {
        // Если отключена, обнуляем nextRunTime чтобы планировщик её пропустил
        t->nextRunTime = std::chrono::system_clock::time_point{};
    }

    // Обновляем задачу в менеджере
    taskManager->UpdateTask(t);

    // Уведомляем планировщик
    scheduler->Notify();

    // Обновляем UI
    RefreshList();

    // Показываем уведомление пользователю
    std::wstring msg = L"Task '" + t->name + L"' is now " + (t->enabled ? L"ENABLED" : L"DISABLED");
    MessageBoxW(hwnd, msg.c_str(), L"Status Changed", MB_OK | MB_ICONINFORMATION);
}

LRESULT CALLBACK MainWindow::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    MainWindow* wnd = nullptr;
    if (uMsg == WM_CREATE) {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
        wnd = (MainWindow*)cs->lpCreateParams;
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)wnd);
        wnd->hwnd = hWnd;
        wnd->CreateControls();
        return 0;
    }
    wnd = (MainWindow*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!wnd) return DefWindowProcW(hWnd, uMsg, wParam, lParam);

    switch (uMsg) {
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case 2001: wnd->OnNew(); break;
        case 2002: wnd->OnEdit(); break;
        case 2003: wnd->OnDelete(); break;
        case 2004: wnd->OnRun(); break;
        case 2005: wnd->RefreshList(); break;
        case 2006: wnd->OnToggleEnabled(); break;  // ? ДОБАВЛЕНО
        }
        break;
    case WM_USER + 100:
        wnd->RefreshList();
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}