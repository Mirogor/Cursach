// TaskDialog.cpp — исправленная версия
#include "TaskDialog.h"
#include "Utils.h"
#include "Logger.h"

#include <windows.h>
#include <commctrl.h>
#include <shobjidl.h>   // IFileOpenDialog
#include <shlwapi.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include <bitset>
#include <algorithm>
#include <memory>

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Ole32.lib")

// Контролы ID
enum {
    IDC_ED_NAME = 1001,
    IDC_ED_EXE,
    IDC_BTN_BROWSE_EXE,
    IDC_ED_ARGS,
    IDC_ED_WORKDIR,
    IDC_BTN_BROWSE_WORKDIR,
    IDC_CB_ENABLED,
    IDC_CB_TRIGGER,
    IDC_ED_INTERVAL,
    IDC_ED_DAILY_HOUR,
    IDC_ED_DAILY_MIN,
    IDC_CHK_WEEK_SUN,
    IDC_CHK_WEEK_MON,
    IDC_CHK_WEEK_TUE,
    IDC_CHK_WEEK_WED,
    IDC_CHK_WEEK_THU,
    IDC_CHK_WEEK_FRI,
    IDC_CHK_WEEK_SAT,
    IDC_ED_WEEKLY_HOUR,
    IDC_ED_WEEKLY_MIN,
    IDC_BTN_OK,
    IDC_BTN_CANCEL
};

// Размер клиентской области диалога (пиксели)
static const int CLIENT_W = 640;
static const int CLIENT_H = 380;

struct DialogContext {
    HWND hwnd;
    HWND parent;
    TaskPtr task;
    bool isNew;
    bool done = false;
    int result = IDCANCEL;
};

// Вспомогательные функции
static void SetTextToEdit(HWND hEd, const std::wstring& s) {
    SetWindowTextW(hEd, s.c_str());
}
static std::wstring GetTextFromEdit(HWND hEd) {
    int len = GetWindowTextLengthW(hEd);
    std::wstring s(len, L'\0');
    if (len > 0) {
        GetWindowTextW(hEd, &s[0], len + 1);
        // remove any trailing null
        if (!s.empty() && s.back() == L'\0') s.resize(len);
    }
    else {
        s.clear();
    }
    return s;
}

// Выбор исполняемого файла
static BOOL BrowseForFile(HWND owner, std::wstring& outPath) {
    OPENFILENAMEW ofn{};
    wchar_t szFile[MAX_PATH] = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Executables\0*.exe\0All files\0*.*\0";
    ofn.lpstrTitle = L"Select executable";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&ofn)) {
        outPath = szFile;
        return TRUE;
    }
    return FALSE;
}

// Выбор каталога — через IFileOpenDialog с FOS_PICKFOLDERS
static BOOL BrowseForFolder(HWND owner, std::wstring& outPath) {
    // Предполагаем, что COM инициализирован в основном потоке (но защищаемся)
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool coInit = SUCCEEDED(hr);

    IFileOpenDialog* pFileOpen = nullptr;
    hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFileOpen));
    if (FAILED(hr) || !pFileOpen) {
        if (coInit) CoUninitialize();
        return FALSE;
    }

    DWORD options = 0;
    if (SUCCEEDED(pFileOpen->GetOptions(&options))) {
        pFileOpen->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    }

    hr = pFileOpen->Show(owner);
    if (FAILED(hr)) {
        pFileOpen->Release();
        if (coInit) CoUninitialize();
        return FALSE;
    }

    IShellItem* pItem = nullptr;
    hr = pFileOpen->GetResult(&pItem);
    if (SUCCEEDED(hr) && pItem) {
        PWSTR pszPath = nullptr;
        hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
        if (SUCCEEDED(hr) && pszPath) {
            outPath.assign(pszPath);
            CoTaskMemFree(pszPath);
            pItem->Release();
            pFileOpen->Release();
            if (coInit) CoUninitialize();
            return TRUE;
        }
        pItem->Release();
    }

    pFileOpen->Release();
    if (coInit) CoUninitialize();
    return FALSE;
}

// Создание контролов (используем client coords)
static void CreateControls(DialogContext* ctx) {
    HWND hwnd = ctx->hwnd;
    const int margin = 10;
    int x = margin, y = margin;

    // Name
    CreateWindowW(L"STATIC", L"Name:", WS_CHILD | WS_VISIBLE, x, y, 80, 20, hwnd, NULL, NULL, NULL);
    HWND hEdName = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_LEFT,
        x + 90, y, 520, 22, hwnd, (HMENU)IDC_ED_NAME, NULL, NULL);
    y += 30;

    // Exe path + browse
    CreateWindowW(L"STATIC", L"Executable:", WS_CHILD | WS_VISIBLE, x, y, 80, 20, hwnd, NULL, NULL, NULL);
    HWND hEdExe = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_LEFT,
        x + 90, y, 420, 22, hwnd, (HMENU)IDC_ED_EXE, NULL, NULL);
    CreateWindowW(L"BUTTON", L"...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 520, y, 30, 22, hwnd, (HMENU)IDC_BTN_BROWSE_EXE, NULL, NULL);
    y += 30;

    // Arguments
    CreateWindowW(L"STATIC", L"Arguments:", WS_CHILD | WS_VISIBLE, x, y, 80, 20, hwnd, NULL, NULL, NULL);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_LEFT,
        x + 90, y, 520, 22, hwnd, (HMENU)IDC_ED_ARGS, NULL, NULL);
    y += 30;

    // Working dir + browse
    CreateWindowW(L"STATIC", L"Working dir:", WS_CHILD | WS_VISIBLE, x, y, 80, 20, hwnd, NULL, NULL, NULL);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_LEFT,
        x + 90, y, 420, 22, hwnd, (HMENU)IDC_ED_WORKDIR, NULL, NULL);
    CreateWindowW(L"BUTTON", L"...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 520, y, 30, 22, hwnd, (HMENU)IDC_BTN_BROWSE_WORKDIR, NULL, NULL);
    y += 30;

    // Enabled
    CreateWindowW(L"BUTTON", L"Enabled", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        x + 90, y, 120, 22, hwnd, (HMENU)IDC_CB_ENABLED, NULL, NULL);
    y += 30;

    // Trigger type
    CreateWindowW(L"STATIC", L"Trigger:", WS_CHILD | WS_VISIBLE, x, y, 80, 20, hwnd, NULL, NULL, NULL);
    HWND hCbTrigger = CreateWindowExW(0, WC_COMBOBOXW, NULL,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        x + 90, y, 220, 200, hwnd, (HMENU)IDC_CB_TRIGGER, NULL, NULL);
    SendMessageW(hCbTrigger, CB_ADDSTRING, 0, (LPARAM)L"Once");
    SendMessageW(hCbTrigger, CB_ADDSTRING, 0, (LPARAM)L"Interval");
    SendMessageW(hCbTrigger, CB_ADDSTRING, 0, (LPARAM)L"Daily");
    SendMessageW(hCbTrigger, CB_ADDSTRING, 0, (LPARAM)L"Weekly");
    SendMessageW(hCbTrigger, CB_SETCURSEL, 1, 0); // default Interval
    y += 30;

    // Interval minutes
    CreateWindowW(L"STATIC", L"Interval (min):", WS_CHILD | WS_VISIBLE, x + 100, y, 120, 20, hwnd, NULL, NULL, NULL);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_NUMBER,
        x + 220, y, 80, 22, hwnd, (HMENU)IDC_ED_INTERVAL, NULL, NULL);
    y += 30;

    // Daily time
    CreateWindowW(L"STATIC", L"Daily Hour:", WS_CHILD | WS_VISIBLE, x + 100, y, 70, 20, hwnd, NULL, NULL, NULL);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_NUMBER,
        x + 180, y, 40, 22, hwnd, (HMENU)IDC_ED_DAILY_HOUR, NULL, NULL);
    CreateWindowW(L"STATIC", L"Min:", WS_CHILD | WS_VISIBLE, x + 225, y, 30, 20, hwnd, NULL, NULL, NULL);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_NUMBER,
        x + 260, y, 40, 22, hwnd, (HMENU)IDC_ED_DAILY_MIN, NULL, NULL);
    y += 30;

    // Weekly days
    CreateWindowW(L"STATIC", L"Weekly days:", WS_CHILD | WS_VISIBLE, x + 100, y, 90, 20, hwnd, NULL, NULL, NULL);
    CreateWindowW(L"BUTTON", L"Sun", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, x + 200, y, 50, 22, hwnd, (HMENU)IDC_CHK_WEEK_SUN, NULL, NULL);
    CreateWindowW(L"BUTTON", L"Mon", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, x + 260, y, 50, 22, hwnd, (HMENU)IDC_CHK_WEEK_MON, NULL, NULL);
    CreateWindowW(L"BUTTON", L"Tue", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, x + 320, y, 50, 22, hwnd, (HMENU)IDC_CHK_WEEK_TUE, NULL, NULL);
    CreateWindowW(L"BUTTON", L"Wed", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, x + 380, y, 50, 22, hwnd, (HMENU)IDC_CHK_WEEK_WED, NULL, NULL);
    CreateWindowW(L"BUTTON", L"Thu", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, x + 440, y, 50, 22, hwnd, (HMENU)IDC_CHK_WEEK_THU, NULL, NULL);
    CreateWindowW(L"BUTTON", L"Fri", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, x + 500, y, 50, 22, hwnd, (HMENU)IDC_CHK_WEEK_FRI, NULL, NULL);
    CreateWindowW(L"BUTTON", L"Sat", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, x + 560, y, 60, 22, hwnd, (HMENU)IDC_CHK_WEEK_SAT, NULL, NULL);
    y += 30;

    // Weekly time
    CreateWindowW(L"STATIC", L"Weekly Hour:", WS_CHILD | WS_VISIBLE, x + 100, y, 80, 20, hwnd, NULL, NULL, NULL);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_NUMBER,
        x + 190, y, 40, 22, hwnd, (HMENU)IDC_ED_WEEKLY_HOUR, NULL, NULL);
    CreateWindowW(L"STATIC", L"Min:", WS_CHILD | WS_VISIBLE, x + 235, y, 30, 20, hwnd, NULL, NULL, NULL);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_NUMBER,
        x + 270, y, 40, 22, hwnd, (HMENU)IDC_ED_WEEKLY_MIN, NULL, NULL);
    y += 40;

    // Buttons — разместим их относительно client area, гарантировано видимые
    HWND hBtnOK = CreateWindowW(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        CLIENT_W - 180, CLIENT_H - 40, 80, 28, hwnd, (HMENU)IDC_BTN_OK, NULL, NULL);
    HWND hBtnCancel = CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CLIENT_W - 90, CLIENT_H - 40, 80, 28, hwnd, (HMENU)IDC_BTN_CANCEL, NULL, NULL);

    // Fill values from task if present
    if (ctx->task) {
        SetTextToEdit(hEdName, ctx->task->name);
        SetTextToEdit(hEdExe, ctx->task->exePath);
        SetTextToEdit(GetDlgItem(hwnd, IDC_ED_ARGS), ctx->task->arguments);
        SetTextToEdit(GetDlgItem(hwnd, IDC_ED_WORKDIR), ctx->task->workingDirectory);
        SendMessageW(GetDlgItem(hwnd, IDC_CB_ENABLED), BM_SETCHECK, ctx->task->enabled ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(GetDlgItem(hwnd, IDC_CB_TRIGGER), CB_SETCURSEL, (WPARAM)ctx->task->triggerType, 0);
        SetTextToEdit(GetDlgItem(hwnd, IDC_ED_INTERVAL), std::to_wstring(ctx->task->intervalMinutes));
        SetTextToEdit(GetDlgItem(hwnd, IDC_ED_DAILY_HOUR), std::to_wstring(ctx->task->dailyHour));
        SetTextToEdit(GetDlgItem(hwnd, IDC_ED_DAILY_MIN), std::to_wstring(ctx->task->dailyMinute));
        SendMessageW(GetDlgItem(hwnd, IDC_CHK_WEEK_SUN), BM_SETCHECK, ctx->task->weeklyDays.test(0) ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(GetDlgItem(hwnd, IDC_CHK_WEEK_MON), BM_SETCHECK, ctx->task->weeklyDays.test(1) ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(GetDlgItem(hwnd, IDC_CHK_WEEK_TUE), BM_SETCHECK, ctx->task->weeklyDays.test(2) ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(GetDlgItem(hwnd, IDC_CHK_WEEK_WED), BM_SETCHECK, ctx->task->weeklyDays.test(3) ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(GetDlgItem(hwnd, IDC_CHK_WEEK_THU), BM_SETCHECK, ctx->task->weeklyDays.test(4) ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(GetDlgItem(hwnd, IDC_CHK_WEEK_FRI), BM_SETCHECK, ctx->task->weeklyDays.test(5) ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(GetDlgItem(hwnd, IDC_CHK_WEEK_SAT), BM_SETCHECK, ctx->task->weeklyDays.test(6) ? BST_CHECKED : BST_UNCHECKED, 0);
        SetTextToEdit(GetDlgItem(hwnd, IDC_ED_WEEKLY_HOUR), std::to_wstring(ctx->task->weeklyHour));
        SetTextToEdit(GetDlgItem(hwnd, IDC_ED_WEEKLY_MIN), std::to_wstring(ctx->task->weeklyMinute));
    }
    else {
        SendMessageW(GetDlgItem(hwnd, IDC_CB_ENABLED), BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(GetDlgItem(hwnd, IDC_CB_TRIGGER), CB_SETCURSEL, 1, 0);
        SetTextToEdit(GetDlgItem(hwnd, IDC_ED_INTERVAL), L"60");
        SetTextToEdit(GetDlgItem(hwnd, IDC_ED_DAILY_HOUR), L"12");
        SetTextToEdit(GetDlgItem(hwnd, IDC_ED_DAILY_MIN), L"0");
        SendMessageW(GetDlgItem(hwnd, IDC_CHK_WEEK_SUN), BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(GetDlgItem(hwnd, IDC_CHK_WEEK_MON), BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(GetDlgItem(hwnd, IDC_CHK_WEEK_TUE), BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(GetDlgItem(hwnd, IDC_CHK_WEEK_WED), BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(GetDlgItem(hwnd, IDC_CHK_WEEK_THU), BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(GetDlgItem(hwnd, IDC_CHK_WEEK_FRI), BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(GetDlgItem(hwnd, IDC_CHK_WEEK_SAT), BM_SETCHECK, BST_CHECKED, 0);
        SetTextToEdit(GetDlgItem(hwnd, IDC_ED_WEEKLY_HOUR), L"12");
        SetTextToEdit(GetDlgItem(hwnd, IDC_ED_WEEKLY_MIN), L"0");
    }
}

// Обработчик диалога
static LRESULT CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DialogContext* ctx = (DialogContext*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
        ctx = (DialogContext*)cs->lpCreateParams;
        ctx->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)ctx);
        CreateControls(ctx);
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == IDC_BTN_BROWSE_EXE) {
            std::wstring path;
            if (BrowseForFile(hwnd, path)) {
                SetTextToEdit(GetDlgItem(hwnd, IDC_ED_EXE), path);
            }
            return 0;
        }
        if (id == IDC_BTN_BROWSE_WORKDIR) {
            std::wstring path;
            if (BrowseForFolder(hwnd, path)) {
                SetTextToEdit(GetDlgItem(hwnd, IDC_ED_WORKDIR), path);
            }
            return 0;
        }
        if (id == IDC_BTN_OK) {
            // Сбор данных в ctx->task
            if (!ctx->task) ctx->task = std::make_shared<Task>();

            ctx->task->name = GetTextFromEdit(GetDlgItem(hwnd, IDC_ED_NAME));
            ctx->task->exePath = GetTextFromEdit(GetDlgItem(hwnd, IDC_ED_EXE));
            ctx->task->arguments = GetTextFromEdit(GetDlgItem(hwnd, IDC_ED_ARGS));
            ctx->task->workingDirectory = GetTextFromEdit(GetDlgItem(hwnd, IDC_ED_WORKDIR));
            ctx->task->enabled = (SendMessageW(GetDlgItem(hwnd, IDC_CB_ENABLED), BM_GETCHECK, 0, 0) == BST_CHECKED);

            int trig = (int)SendMessageW(GetDlgItem(hwnd, IDC_CB_TRIGGER), CB_GETCURSEL, 0, 0);
            ctx->task->triggerType = (TriggerType)trig;

            // interval
            std::wstring sInterval = GetTextFromEdit(GetDlgItem(hwnd, IDC_ED_INTERVAL));
            ctx->task->intervalMinutes = (uint32_t)(_wtoi(sInterval.c_str()));

            // daily
            std::wstring sDh = GetTextFromEdit(GetDlgItem(hwnd, IDC_ED_DAILY_HOUR));
            std::wstring sDm = GetTextFromEdit(GetDlgItem(hwnd, IDC_ED_DAILY_MIN));
            ctx->task->dailyHour = (uint8_t)std::clamp(_wtoi(sDh.c_str()), 0, 23);
            ctx->task->dailyMinute = (uint8_t)std::clamp(_wtoi(sDm.c_str()), 0, 59);

            // weekly days
            std::bitset<7> bs;
            bs.set(0, SendMessageW(GetDlgItem(hwnd, IDC_CHK_WEEK_SUN), BM_GETCHECK, 0, 0) == BST_CHECKED);
            bs.set(1, SendMessageW(GetDlgItem(hwnd, IDC_CHK_WEEK_MON), BM_GETCHECK, 0, 0) == BST_CHECKED);
            bs.set(2, SendMessageW(GetDlgItem(hwnd, IDC_CHK_WEEK_TUE), BM_GETCHECK, 0, 0) == BST_CHECKED);
            bs.set(3, SendMessageW(GetDlgItem(hwnd, IDC_CHK_WEEK_WED), BM_GETCHECK, 0, 0) == BST_CHECKED);
            bs.set(4, SendMessageW(GetDlgItem(hwnd, IDC_CHK_WEEK_THU), BM_GETCHECK, 0, 0) == BST_CHECKED);
            bs.set(5, SendMessageW(GetDlgItem(hwnd, IDC_CHK_WEEK_FRI), BM_GETCHECK, 0, 0) == BST_CHECKED);
            bs.set(6, SendMessageW(GetDlgItem(hwnd, IDC_CHK_WEEK_SAT), BM_GETCHECK, 0, 0) == BST_CHECKED);
            ctx->task->weeklyDays = bs;

            // weekly time
            std::wstring sWh = GetTextFromEdit(GetDlgItem(hwnd, IDC_ED_WEEKLY_HOUR));
            std::wstring sWm = GetTextFromEdit(GetDlgItem(hwnd, IDC_ED_WEEKLY_MIN));
            ctx->task->weeklyHour = (uint8_t)std::clamp(_wtoi(sWh.c_str()), 0, 23);
            ctx->task->weeklyMinute = (uint8_t)std::clamp(_wtoi(sWm.c_str()), 0, 59);

            // ensure id if needed
            if (ctx->isNew || ctx->task->id.empty()) {
                ctx->task->id = util::GenerateGUID();
            }

            ctx->done = true;
            ctx->result = IDOK;
            DestroyWindow(hwnd); // окно закроется, modal loop увидит done
            return 0;
        }
        if (id == IDC_BTN_CANCEL) {
            ctx->done = true;
            ctx->result = IDCANCEL;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY: {
        // ничего лишнего, просто оставим окно разрушенным — modal loop узнает по ctx->done
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// Запуск модального диалога
bool TaskDialog::ShowDialog(HWND parent, TaskPtr& task, bool isNew) {
    HINSTANCE hInst = (HINSTANCE)GetModuleHandle(NULL);

    // Регистрируем класс диалога (один раз)
    static ATOM cls = 0;
    const wchar_t CLASS_NAME[] = L"MiniTaskDlgClass_v2";
    if (!cls) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = DialogProc;
        wc.hInstance = hInst;
        wc.lpszClassName = CLASS_NAME;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        cls = RegisterClassW(&wc);
    }

    // Рассчитать внешний размер окна, чтобы client area = CLIENT_W x CLIENT_H
    RECT rc = { 0, 0, CLIENT_W, CLIENT_H };
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    AdjustWindowRect(&rc, style, FALSE);
    int winW = rc.right - rc.left;
    int winH = rc.bottom - rc.top;

    DialogContext ctx{};
    ctx.parent = parent;
    ctx.task = task;
    ctx.isNew = isNew;
    ctx.done = false;
    ctx.result = IDCANCEL;

    HWND hwndDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, CLASS_NAME, L"Edit Task",
        style, CW_USEDEFAULT, CW_USEDEFAULT, winW, winH,
        parent, NULL, hInst, &ctx);

    if (!hwndDlg) return false;

    // Модальность: отключаем родителя
    if (parent) EnableWindow(parent, FALSE);
    ShowWindow(hwndDlg, SW_SHOW);
    UpdateWindow(hwndDlg);

    // modal message loop — прерываем, когда ctx.done == true
    MSG msg;
    while (!ctx.done && GetMessageW(&msg, NULL, 0, 0)) {
        if (IsWindow(hwndDlg) && IsDialogMessageW(hwndDlg, &msg)) {
            // handled
        }
        else {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    // Если окно всё ещё существует — уничтожаем
    if (IsWindow(hwndDlg)) DestroyWindow(hwndDlg);

    // Включаем родителя обратно
    if (parent) {
        EnableWindow(parent, TRUE);
        SetForegroundWindow(parent);
    }

    bool ok = false;
    if (ctx.done && ctx.result == IDOK && ctx.task && !ctx.task->name.empty() && !ctx.task->exePath.empty()) {
        task = ctx.task;
        ok = true;
    }

    return ok;
}
