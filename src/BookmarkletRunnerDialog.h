#pragma once
#include <windows.h>
#include <functional>
#include <string>

// Port of BookmarkletRunner.kt. Runs a user-supplied "javascript:..."
// bookmarklet against the active tab - strips the javascript: prefix (and
// a leading void(0); idiom some bookmarklets use) and hands the rest to
// the same ExecFn plumbing JsConsoleDialog/AccessibilityCheckerDialog use.
class BookmarkletRunnerDialog {
public:
    using ExecFn = std::function<void(const std::wstring& script,
                                       std::function<void(const std::wstring&)>)>;
    static void ShowFor(HWND owner, ExecFn exec);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
    static void Run(HWND hwnd);
};
