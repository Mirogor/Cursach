#include "TaskDialog.h"
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shobjidl.h>
#include <filesystem>
#include "Logger.h"
#include "resource.h"
#include "Utils.h"

#pragma comment(lib, "Comctl32.lib")

using namespace std;

static TaskPtr g_task;
static bool g_isNew;

static void ShowCtrl(HWND hDlg, int id, bool show)
{
    HWND h = GetDlgItem(hDlg, id);
    if (h) ShowWindow(h, show ? SW_SHOW : SW_HIDE);
}

static void UpdateTriggerUI(HWND hDlg)
{
    int t = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_TASK_TRIGGER));

    // Скрываем ВСЕ контролы
    ShowCtrl(hDlg, IDC_TASK_INTERVAL, false);
    ShowCtrl(hDlg, IDC_INTERVAL_LABEL, false);  // ← ДОБАВЛЕНО: скрываем метку

    ShowCtrl(hDlg, IDC_TASK_TIME, false);

    ShowCtrl(hDlg, IDC_TASK_ONCE_DATE, false);
    ShowCtrl(hDlg, IDC_TASK_ONCE_TIME, false);
    ShowCtrl(hDlg, IDC_ONCE_DATE_LABEL, false);  // ← ДОБАВЛЕНО
    ShowCtrl(hDlg, IDC_ONCE_TIME_LABEL, false);  // ← ДОБАВЛЕНО

    ShowCtrl(hDlg, IDC_DAY_MON, false);
    ShowCtrl(hDlg, IDC_DAY_TUE, false);
    ShowCtrl(hDlg, IDC_DAY_WED, false);
    ShowCtrl(hDlg, IDC_DAY_THU, false);
    ShowCtrl(hDlg, IDC_DAY_FRI, false);
    ShowCtrl(hDlg, IDC_DAY_SAT, false);
    ShowCtrl(hDlg, IDC_DAY_SUN, false);

    switch (t)
    {
    case (int)TriggerType::ONCE:
        ShowCtrl(hDlg, IDC_TASK_ONCE_DATE, true);
        ShowCtrl(hDlg, IDC_TASK_ONCE_TIME, true);
        ShowCtrl(hDlg, IDC_ONCE_DATE_LABEL, true);  // ← ДОБАВЛЕНО
        ShowCtrl(hDlg, IDC_ONCE_TIME_LABEL, true);  // ← ДОБАВЛЕНО
        break;

    case (int)TriggerType::INTERVAL:
        ShowCtrl(hDlg, IDC_TASK_INTERVAL, true);
        ShowCtrl(hDlg, IDC_INTERVAL_LABEL, true);  // ← ДОБАВЛЕНО
        break;

    case (int)TriggerType::DAILY:
        ShowCtrl(hDlg, IDC_TASK_TIME, true);
        break;

    case (int)TriggerType::WEEKLY:
        ShowCtrl(hDlg, IDC_TASK_TIME, true);
        ShowCtrl(hDlg, IDC_DAY_MON, true);
        ShowCtrl(hDlg, IDC_DAY_TUE, true);
        ShowCtrl(hDlg, IDC_DAY_WED, true);
        ShowCtrl(hDlg, IDC_DAY_THU, true);
        ShowCtrl(hDlg, IDC_DAY_FRI, true);
        ShowCtrl(hDlg, IDC_DAY_SAT, true);
        ShowCtrl(hDlg, IDC_DAY_SUN, true);
        break;
    }
}

static void LoadOnceDateTime(HWND hDlg)
{
    SYSTEMTIME st{};

    if (g_task->runOnceTime.time_since_epoch().count() == 0) {
        GetLocalTime(&st);
        st.wHour = (st.wHour + 1) % 24;
    }
    else {
        time_t tt = std::chrono::system_clock::to_time_t(g_task->runOnceTime);
        tm local{};
        localtime_s(&local, &tt);

        st.wYear = local.tm_year + 1900;
        st.wMonth = local.tm_mon + 1;
        st.wDay = local.tm_mday;
        st.wHour = local.tm_hour;
        st.wMinute = local.tm_min;
        st.wSecond = local.tm_sec;
    }

    DateTime_SetSystemtime(GetDlgItem(hDlg, IDC_TASK_ONCE_DATE), GDT_VALID, &st);
    DateTime_SetSystemtime(GetDlgItem(hDlg, IDC_TASK_ONCE_TIME), GDT_VALID, &st);
}

static void SaveOnceDateTime(HWND hDlg)
{
    SYSTEMTIME stDate{}, stTime{};
    DateTime_GetSystemtime(GetDlgItem(hDlg, IDC_TASK_ONCE_DATE), &stDate);
    DateTime_GetSystemtime(GetDlgItem(hDlg, IDC_TASK_ONCE_TIME), &stTime);

    stDate.wHour = stTime.wHour;
    stDate.wMinute = stTime.wMinute;
    stDate.wSecond = stTime.wSecond;

    tm local{};
    local.tm_year = stDate.wYear - 1900;
    local.tm_mon = stDate.wMonth - 1;
    local.tm_mday = stDate.wDay;
    local.tm_hour = stDate.wHour;
    local.tm_min = stDate.wMinute;
    local.tm_sec = stDate.wSecond;
    local.tm_isdst = -1;

    time_t tt = mktime(&local);
    g_task->runOnceTime = std::chrono::system_clock::from_time_t(tt);
}

static void LoadTime(HWND hDlg)
{
    SYSTEMTIME st{};
    GetLocalTime(&st);

    st.wHour = g_task->dailyHour;
    st.wMinute = g_task->dailyMinute;
    st.wSecond = g_task->dailySecond;

    DateTime_SetSystemtime(GetDlgItem(hDlg, IDC_TASK_TIME), GDT_VALID, &st);
}

static void SaveTime(HWND hDlg)
{
    SYSTEMTIME st{};
    DateTime_GetSystemtime(GetDlgItem(hDlg, IDC_TASK_TIME), &st);
    g_task->dailyHour = st.wHour;
    g_task->dailyMinute = st.wMinute;
    g_task->dailySecond = st.wSecond;

    g_task->weeklyHour = st.wHour;
    g_task->weeklyMinute = st.wMinute;
    g_task->weeklySecond = st.wSecond;
}

static void LoadWeekdays(HWND hDlg)
{
    auto load = [&](int id, int bit)
        {
            CheckDlgButton(hDlg, id, g_task->weeklyDays.test(bit) ? BST_CHECKED : BST_UNCHECKED);
        };

    load(IDC_DAY_MON, 1);
    load(IDC_DAY_TUE, 2);
    load(IDC_DAY_WED, 3);
    load(IDC_DAY_THU, 4);
    load(IDC_DAY_FRI, 5);
    load(IDC_DAY_SAT, 6);
    load(IDC_DAY_SUN, 0);
}

static void SaveWeekdays(HWND hDlg)
{
    auto save = [&](int id, int bit)
        {
            if (IsDlgButtonChecked(hDlg, id) == BST_CHECKED)
                g_task->weeklyDays.set(bit, true);
            else
                g_task->weeklyDays.set(bit, false);
        };

    save(IDC_DAY_MON, 1);
    save(IDC_DAY_TUE, 2);
    save(IDC_DAY_WED, 3);
    save(IDC_DAY_THU, 4);
    save(IDC_DAY_FRI, 5);
    save(IDC_DAY_SAT, 6);
    save(IDC_DAY_SUN, 0);
}

static bool ValidateFields(HWND hDlg)
{
    wchar_t name[256], exe[512];
    GetDlgItemTextW(hDlg, IDC_TASK_NAME, name, 256);
    GetDlgItemTextW(hDlg, IDC_TASK_EXE, exe, 512);

    if (wcslen(name) == 0)
    {
        MessageBoxW(hDlg, L"Name cannot be empty.", L"Error", MB_ICONERROR);
        return false;
    }

    if (wcslen(exe) == 0)
    {
        MessageBoxW(hDlg, L"Executable path cannot be empty.", L"Error", MB_ICONERROR);
        return false;
    }

    if (!filesystem::exists(exe))
    {
        MessageBoxW(hDlg, L"File does not exist.", L"Error", MB_ICONERROR);
        return false;
    }

    return true;
}

static void BrowseForExe(HWND hDlg)
{
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&dlg))))
        return;

    dlg->SetTitle(L"Select executable (*.exe)");

    const COMDLG_FILTERSPEC filter[] = {
        { L"Executable", L"*.exe" }
    };
    dlg->SetFileTypes(1, filter);

    if (SUCCEEDED(dlg->Show(hDlg)))
    {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item)))
        {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
            {
                SetDlgItemTextW(hDlg, IDC_TASK_EXE, path);
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }

    dlg->Release();
}

static void LoadTaskToDialog(HWND hDlg)
{
    SetDlgItemTextW(hDlg, IDC_TASK_NAME, g_task->name.c_str());
    SetDlgItemTextW(hDlg, IDC_TASK_EXE, g_task->exePath.c_str());

    HWND cb = GetDlgItem(hDlg, IDC_TASK_TRIGGER);
    ComboBox_AddString(cb, L"Once");
    ComboBox_AddString(cb, L"Interval");
    ComboBox_AddString(cb, L"Daily");
    ComboBox_AddString(cb, L"Weekly");

    ComboBox_SetCurSel(cb, (int)g_task->triggerType);

    SetDlgItemInt(hDlg, IDC_TASK_INTERVAL, g_task->intervalMinutes, FALSE);
}

static void SaveTask(HWND hDlg)
{
    wchar_t buf[512];

    GetDlgItemTextW(hDlg, IDC_TASK_NAME, buf, 512);
    g_task->name = buf;

    GetDlgItemTextW(hDlg, IDC_TASK_EXE, buf, 512);
    g_task->exePath = buf;

    g_task->triggerType =
        (TriggerType)ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_TASK_TRIGGER));

    switch (g_task->triggerType) {
    case TriggerType::ONCE:
        SaveOnceDateTime(hDlg);
        break;
    case TriggerType::INTERVAL:
        g_task->intervalMinutes = GetDlgItemInt(hDlg, IDC_TASK_INTERVAL, nullptr, FALSE);
        break;
    case TriggerType::DAILY:
        SaveTime(hDlg);
        break;
    case TriggerType::WEEKLY:
        SaveTime(hDlg);
        SaveWeekdays(hDlg);
        break;
    }
}

static HBRUSH hGreen = CreateSolidBrush(RGB(210, 255, 210));
static HBRUSH hRed = CreateSolidBrush(RGB(255, 210, 210));
static HBRUSH hWhite = CreateSolidBrush(RGB(255, 255, 255));

static bool g_exeValid = true;

static INT_PTR CALLBACK DlgProc(HWND hDlg, UINT msg, WPARAM w, LPARAM l)
{
    switch (msg)
    {
    case WM_INITDIALOG:
        LoadTaskToDialog(hDlg);
        LoadTime(hDlg);
        LoadWeekdays(hDlg);
        LoadOnceDateTime(hDlg);
        UpdateTriggerUI(hDlg);
        return TRUE;

    case WM_COMMAND:
        if (LOWORD(w) == IDC_TASK_BROWSE)
        {
            BrowseForExe(hDlg);
            return TRUE;
        }

        if (LOWORD(w) == IDC_TASK_TRIGGER && HIWORD(w) == CBN_SELCHANGE)
        {
            UpdateTriggerUI(hDlg);
            return TRUE;
        }

        if (LOWORD(w) == IDC_TASK_EXE && HIWORD(w) == EN_CHANGE)
        {
            wchar_t exe[512];
            GetDlgItemTextW(hDlg, IDC_TASK_EXE, exe, 512);
            g_exeValid = filesystem::exists(exe);
            InvalidateRect(hDlg, NULL, TRUE);
        }

        if (LOWORD(w) == IDOK)
        {
            if (!ValidateFields(hDlg))
                return TRUE;

            SaveTask(hDlg);
            EndDialog(hDlg, 1);
            return TRUE;
        }

        if (LOWORD(w) == IDCANCEL)
        {
            EndDialog(hDlg, 0);
            return TRUE;
        }
        break;

    case WM_CTLCOLOREDIT:
    {
        HWND hCtrl = (HWND)l;
        if (GetDlgCtrlID(hCtrl) == IDC_TASK_EXE)
        {
            HDC hdc = (HDC)w;
            SetBkMode(hdc, TRANSPARENT);
            return (INT_PTR)(g_exeValid ? hGreen : hRed);
        }
        break;
    }
    }

    return FALSE;
}

bool TaskDialog::ShowDialog(HWND parent, TaskPtr& task, bool isNew)
{
    g_task = isNew ? make_shared<Task>() : task;
    g_isNew = isNew;

    if (isNew)
        g_task->id = util::GenerateGUID();

    INT_PTR r = DialogBoxParamW(
        GetModuleHandleW(nullptr),
        MAKEINTRESOURCEW(IDD_TASK_DIALOG),
        parent,
        DlgProc,
        0);

    if (r == 1)
    {
        task = g_task;
        return true;
    }

    return false;
}