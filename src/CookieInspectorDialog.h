#pragma once
#include <windows.h>
#include <functional>
#include <string>

// Port of CookieInspector.kt. Read-only listing of cookies for the active
// tab's current page, via BlackholeApp::GetCookiesForActiveTab
// (ICoreWebView2CookieManager) - same read-only recon scope as the Kotlin
// version (it never set/deleted cookies either).
class CookieInspectorDialog {
public:
    using GetCookiesFn = std::function<void(std::function<void(const std::wstring&)>)>;
    static void ShowFor(HWND owner, GetCookiesFn getCookies);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
};
