#pragma once
#include <windows.h>
#include <functional>
#include <string>

// Port of StorageInspector.kt. Dumps localStorage and sessionStorage for
// the active tab's page via JS injection - same ExecFn plumbing as
// JsConsoleDialog/AccessibilityCheckerDialog. Read-only (never writes to
// storage), same as the Kotlin version.
class StorageInspectorDialog {
public:
    using ExecFn = std::function<void(const std::wstring& script,
                                       std::function<void(const std::wstring&)>)>;
    static void ShowFor(HWND owner, ExecFn exec);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
    static void Refresh(HWND hwnd);
};
