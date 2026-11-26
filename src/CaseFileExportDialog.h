#pragma once
#include <windows.h>

// Port of CaseFileExport.kt. Freeform session-notes editor that exports to
// a timestamped .txt file - same scope as the Kotlin version (it didn't
// auto-pull data from other tools either; it was a notes bundle the user
// filled in themselves).
class CaseFileExportDialog {
public:
    static void ShowFor(HWND owner);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
    static void Export(HWND hwnd);
};
