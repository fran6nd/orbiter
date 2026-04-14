#include "ConsoleManager.h"

#ifdef _WIN32
#include <windows.h>

bool ConsoleManager::IsConsoleExclusive(void) {
    DWORD pids[2];
    DWORD num_pids = GetConsoleProcessList(pids, 2);
    return num_pids <= 1;
}

void ConsoleManager::ShowConsole(bool show)
{
    HWND wnd = GetConsoleWindow();
    if (wnd)
        ShowWindow(wnd, show ? SW_SHOW : SW_HIDE);
}
#endif // _WIN32
