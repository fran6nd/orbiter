#pragma once

class ConsoleManager {
public:
#ifdef _WIN32
    static bool IsConsoleExclusive(void);
    static void ShowConsole(bool show);
#else
    static bool IsConsoleExclusive(void) { return false; }
    static void ShowConsole(bool /*show*/) {}
#endif
};
