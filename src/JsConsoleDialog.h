#pragma once
#include <windows.h>
#include <functional>
#include <string>

// Port of JavaScriptConsole.kt (Advance mode). Runs arbitrary JS in the
// active tab's page context via ICoreWebView2::ExecuteScript, same
// capability the Kotlin version got from WebView.evaluateJavascript.
//
// Decoupled from BlackholeApp/WebView2 via a callback so this file has no
// WebView2 includes of its own - BlackholeApp.cpp wires the callback to
// the active tab's ExecuteScript call.
class JsConsoleDialog {
public:
    using ExecFn = std::function<void(const std::wstring& script,
                                       std::function<void(const std::wstring& resultJson)>)>;

    static void ShowFor(HWND owner, ExecFn exec);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
};
