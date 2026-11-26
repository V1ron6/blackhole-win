#pragma once
#include <windows.h>

// Port of FileInspector.kt. Picks a local file and reports its size, a hex
// dump of the first bytes, and a best-guess type from its magic number -
// same inspection scope as the Kotlin version, purely local/offline.
class FileInspectorDialog {
public:
    static void ShowFor(HWND owner);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
    static void PickAndInspect(HWND hwnd);
};
