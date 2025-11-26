#include "TaskDialog.h"
#include <windows.h>
#include <windowsx.h>   // <-- добавлено для ComboBox_*
#include <shobjidl.h>
#include <filesystem>
#include <string>
#include "Logger.h"
#include "resource.h"
#include "utils.h"

using namespace std;

static TaskPtr g_task;
static bool g_isNew;

static void BrowseForExe(HWND hDlg)
{
    IFileOpenDialog* dlg = nullptr;

    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))))
        return;

    dlg->SetTitle(L"Select executable (.exe)");

    COMDLG_FILTERSPEC filter[] = {
        { L"Executable files", L"*.exe" }
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

static bool ValidateFields(HWND hDlg)
{
    wchar_t name[256], exe[512];
    GetDlgItemTextW(hDlg, IDC_TASK_NAME, name, 256);
    GetDlgItemTextW(hDlg, IDC_TASK_EXE, exe, 512);

    if (wcslen(name) == 0) {
        MessageBoxW(hDlg, L"Name cannot be empty.", L"Error", MB_ICONERROR);
        return false;
    }

    if (wcslen(exe) == 0) {
        MessageBoxW(hDlg, L"Executable path cannot be empty.", L"Error", MB_ICONERROR);
        return false;
    }

    if (!std::filesystem::exists(exe)) {
        MessageBoxW(hDlg, L"File does not exist.", L"Error", MB_ICONERROR);
        return false;
    }

    return true;
}

static void LoadTaskToDialog(HWND hDlg)
{
    SetDlgItemTextW(hDlg, IDC_TASK_NAME, g_task->name.c_str());
    SetDlgItemTextW(hDlg, IDC_TASK_EXE, g_task->exePath.c_str());

    HWND c = GetDlgItem(hDlg, IDC_TASK_TRIGGER);
    ComboBox_AddString(c, L"Once");
    ComboBox_AddString(c, L"Interval");
    ComboBox_AddString(c, L"Daily");
    ComboBox_AddString(c, L"Weekly");

    ComboBox_SetCurSel(c, (int)g_task->triggerType);

    if (g_task->intervalMinutes)
        SetDlgItemInt(hDlg, IDC_TASK_INTERVAL, g_task->intervalMinutes, FALSE);
}

static void SaveDialogToTask(HWND hDlg)
{
    wchar_t buf[512];

    GetDlgItemTextW(hDlg, IDC_TASK_NAME, buf, 512);
    g_task->name = buf;

    GetDlgItemTextW(hDlg, IDC_TASK_EXE, buf, 512);
    g_task->exePath = buf;

    int trig = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_TASK_TRIGGER));
    g_task->triggerType = (TriggerType)trig;

    g_task->intervalMinutes = GetDlgItemInt(hDlg, IDC_TASK_INTERVAL, nullptr, FALSE);
}

static INT_PTR CALLBACK DlgProc(HWND hDlg, UINT msg, WPARAM w, LPARAM l)
{
    switch (msg)
    {
    case WM_INITDIALOG:
        LoadTaskToDialog(hDlg);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(w))
        {
        case IDC_TASK_BROWSE:
            BrowseForExe(hDlg);
            return TRUE;

        case IDOK:
            if (!ValidateFields(hDlg))
                return TRUE;

            SaveDialogToTask(hDlg);
            EndDialog(hDlg, 1);
            return TRUE;

        case IDCANCEL:
            EndDialog(hDlg, 0);
            return TRUE;
        }
        break;
    }

    return FALSE;
}

bool TaskDialog::ShowDialog(HWND parent, TaskPtr& task, bool isNew)
{
    g_isNew = isNew;
    g_task = isNew ? std::make_shared<Task>() : task;

    if (isNew)
        g_task->id = util::GenerateGUID();

    INT_PTR r = DialogBoxParamW(
        GetModuleHandleW(nullptr),
        MAKEINTRESOURCEW(IDD_TASK_DIALOG),
        parent,
        DlgProc,
        0);

    if (r == 1) {
        task = g_task;
        return true;
    }
    return false;
}
