#pragma once
#include <windows.h>
#include <wrl.h>
#include <wrl/client.h>
#include "WebView2.h"
#include <vector>
#include <memory>
#include <string>
#include <functional>
#include "Settings.h"
#include "AdBlocker.h"
#include "DownloadManager.h"

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

// One browser tab = one CoreWebView2Controller + CoreWebView2.
// Mirrors TabManager.kt's per-tab state (js toggle, request log).
struct Tab {
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2>           webview;
    ComPtr<ICoreWebView2Settings>   webviewSettings; // Stage 2: cached so the JS toggle doesn't re-query each click
    bool jsEnabled = false; // Settings.kt: JS off by default, per tab
    std::wstring requestLog; // capped at 200 entries, newest first - RequestLogEntry.kt
    int requestLogCount = 0;
    EventRegistrationToken navStartToken{}, navCompletedToken{}, resourceReqToken{}, newWindowToken{}, downloadToken{};
};

// Owns the top-level window: toolbar (url bar, nav buttons, tab pills,
// settings gear) + up to kMaxTabs WebView2 tabs. Equivalent to
// MainActivity.kt + TabManager.kt combined, since Win32 doesn't split
// "Activity" from "manager" the way Android does.
class BlackholeApp {
public:
    static constexpr int kMaxTabs = 2; // TabManager.kt: hard 2-tab cap

    bool Initialize(HINSTANCE hInstance, int nCmdShow);
    int  RunMessageLoop();

    // Called by whichever tool dialog needs the active tab (JS console).
    void ExecuteScriptOnActiveTab(const std::wstring& script,
                                   std::function<void(const std::wstring&)> callback);

    // Stage 3: used by UserAgentSwitcherDialog. Applies to the active tab
    // only and reloads it so the new UA takes effect on the current page.
    void SetUserAgentOnActiveTab(const std::wstring& ua);

    // Used by the Settings window (same translation unit, see
    // BlackholeApp.cpp's anonymous-namespace SettingsWndProc) to read the
    // current values into its controls and write them back on Save.
    AppSettings& SettingsRef() { return settings_; }
    void OnSettingsSaved(); // persist to disk, refresh mode label, restart download sweeper

private:
    static LRESULT CALLBACK WndProcStatic(HWND, UINT, WPARAM, LPARAM);
    LRESULT WndProc(HWND, UINT, WPARAM, LPARAM);

    void CreateToolbar(HWND parent);
    void CreateWebViewEnvironment();
    void AddTab(const std::wstring& initialUrl = L"");
    void CloseTab(int index);
    void SwitchToTab(int index);
    void LayoutControls();
    void Navigate(const std::wstring& urlOrQuery);
    void UpdateModeIndicator();
    void ApplySecurityModeEnforcement(ICoreWebView2* webview, int tabIndex);
    void HookAdBlocking(Tab& tab);
    void ShowSettingsDialog();
    void OpenToolForCurrentMode(int toolMenuId);
    void HookDownloads(Tab& tab);       // Stage 2
    void ToggleJsForActiveTab();        // Stage 2
    void ShowRequestLogForActiveTab();  // Stage 2
    void UpdateJsToggleButtonText();    // reflects the active tab's jsEnabled

    HWND hwndMain_ = nullptr;
    HWND hwndUrlBar_ = nullptr;
    HWND hwndGo_ = nullptr, hwndBack_ = nullptr, hwndFwd_ = nullptr, hwndReload_ = nullptr, hwndHome_ = nullptr;
    HWND hwndNewTab_ = nullptr, hwndCloseTab_ = nullptr, hwndSettings_ = nullptr, hwndTools_ = nullptr;
    HWND hwndJsToggle_ = nullptr, hwndReqLog_ = nullptr; // Stage 2
    HWND hwndTabButtons_[kMaxTabs] = {};
    HWND hwndModeLabel_ = nullptr;

    ComPtr<ICoreWebView2Environment> environment_;
    std::vector<std::unique_ptr<Tab>> tabs_;
    int activeTab_ = -1;

    AppSettings settings_;
    AdBlocker adBlocker_;
    DownloadManager downloadManager_;
    std::wstring exeDir_;
};
