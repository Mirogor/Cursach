#pragma once
#include <windows.h>
#include <string>
#include <memory>
#include "Task.h"

class TaskDialog {
public:
    // Показывает модальный диалог. parent - обработчик родительского окна.
    // task - если isNew == true, будет создана новая задача; иначе передается существующий TaskPtr и редактируется.
    // Возвращает true при OK (задача изменена/создана), false при Cancel.
    static bool ShowDialog(HWND parent, TaskPtr& task, bool isNew);
};
