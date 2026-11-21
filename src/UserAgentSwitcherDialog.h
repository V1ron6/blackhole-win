#pragma once
#include <windows.h>
#include <functional>
#include <string>

// Port of UserAgentSwitcher.kt. Applies a chosen or custom user-agent
// string to the active tab via ICoreWebView2Settings2::put_UserAgent
// (wired up by BlackholeApp.cpp's applyFn, same decoupling pattern as
// JsConsoleDialog's ExecFn - this header has no WebView2 dependency).
class UserAgentSwitcherDialog {
public:
    static void ShowFor(HWND owner, std::function<void(const std::wstring&)> applyFn);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
};
