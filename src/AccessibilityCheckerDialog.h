#pragma once
#include <windows.h>
#include <functional>
#include <string>

// Port of AccessibilityChecker.kt. Injects a small JS snippet into the
// active tab that scans the DOM for common accessibility gaps (images
// missing alt text, inputs missing labels, missing <html lang>, missing
// <title>) and reports counts - same decoupled ExecFn pattern as
// JsConsoleDialog, so this header stays free of WebView2 types.
class AccessibilityCheckerDialog {
public:
    using ExecFn = std::function<void(const std::wstring& script,
                                       std::function<void(const std::wstring&)>)>;
    static void ShowFor(HWND owner, ExecFn exec);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
};
