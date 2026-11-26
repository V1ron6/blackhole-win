#include "BlackholeApp.h"
#include "WebView2EnvironmentOptions.h"
#include "PostmanDialog.h"
#include "JsConsoleDialog.h"
#include "DataToolkitDialog.h"
#include "RequestLogDialog.h"
#include "JsonFormatterDialog.h"
#include "JwtDecoderDialog.h"
#include "RegexTesterDialog.h"
#include "HashIdentifierDialog.h"
#include "TotpGeneratorDialog.h"
#include "UserAgentSwitcherDialog.h"
#include "DnsLookupDialog.h"
#include "WhoisLookupDialog.h"
#include "TlsInspectorDialog.h"
#include "SecurityHeaderScannerDialog.h"
#include "TechFingerprintDialog.h"
#include "RobotsFetchDialog.h"
#include "FaviconHashDialog.h"
#include "AccessibilityCheckerDialog.h"
#include "BookmarkletRunnerDialog.h"
#include "CaseFileExportDialog.h"
#include "CipherSolverDialog.h"
#include "CookieInspectorDialog.h"
#include "DiffViewerDialog.h"
#include "FileInspectorDialog.h"
#include "GraphQLIntrospectionDialog.h"
#include "HarExportDialog.h"
#include "RequestTimelineDialog.h"
#include "ScreenshotCaptureDialog.h"
#include "StorageInspectorDialog.h"
#include "StringsExtractorDialog.h"
#include "ViewportEmulatorDialog.h"
#include "ExploitShellDialog.h"
#include "PortScannerDialog.h"
#include "DirectoryBusterDialog.h"
#include "HashCrackerDialog.h"
#include "SSHTerminalDialog.h"
#include "HydraLiteDialog.h"
#include <commctrl.h>
#include <shlwapi.h>
#include <objidl.h>
#include <sstream>
#include <cwctype>
#include <algorithm>

#pragma comment(lib, "shlwapi.lib")

namespace {
    const wchar_t* kWindowClass = L"BlackholeMainWindow";
    const wchar_t* kHomepageRelPath = L"\\assets\\homepage.html";
    const wchar_t* kAdblockRelPath  = L"\\assets\\adblock_hosts.txt";

    // --- Command IDs -------------------------------------------------------
    enum : int {
        ID_URLBAR = 1001, ID_GO, ID_BACK, ID_FWD, ID_RELOAD, ID_HOME,
        ID_NEWTAB, ID_CLOSETAB, ID_SETTINGS, ID_TOOLS,
        ID_JSTOGGLE, ID_REQLOG, // Stage 2
        ID_TAB_BASE = 1100, // ID_TAB_BASE + index for tab pill buttons

        // Tools menu
        ID_TOOL_HTTP = 1200, ID_TOOL_JSCONSOLE, ID_TOOL_DATATOOLKIT,
        ID_TOOL_JSON, ID_TOOL_JWT, ID_TOOL_REGEX, ID_TOOL_HASHID, ID_TOOL_TOTP, ID_TOOL_USERAGENT, // Stage 3
        ID_TOOL_DNS, ID_TOOL_WHOIS, ID_TOOL_TLS, ID_TOOL_SECHEADERS, ID_TOOL_TECHFP,
        ID_TOOL_ROBOTS, ID_TOOL_FAVICON, // Stage 4
        ID_TOOL_A11Y, ID_TOOL_BOOKMARKLET, ID_TOOL_CASEFILE, ID_TOOL_CIPHER, ID_TOOL_COOKIES,
        ID_TOOL_DIFF, ID_TOOL_FILEINSPECT, ID_TOOL_GRAPHQL, ID_TOOL_HAR, ID_TOOL_TIMELINE,
        ID_TOOL_SCREENSHOT, ID_TOOL_STORAGE, ID_TOOL_STRINGS, ID_TOOL_VIEWPORT, // Stage 5
        ID_TOOL_EXPLOITSHELL, ID_TOOL_HASHCRACK, // Stage 6
        ID_TOOL_SSH_STUB, ID_TOOL_PORTSCAN_STUB, ID_TOOL_HYDRA_STUB, ID_TOOL_DIRBUST_STUB,

        // Settings dialog (separate child window, own ID space)
        ID_SET_MODE = 1300, ID_SET_SECMODE, ID_SET_PROXY_ENABLE, ID_SET_PROXY_SCHEME,
        ID_SET_PROXY_HOST, ID_SET_PROXY_PORT, ID_SET_BURP, ID_SET_RETENTION, ID_SET_SAVE,
    };

    std::wstring GetWindowTextStr(HWND h) {
        int len = GetWindowTextLengthW(h);
        std::wstring s(len, L'\0');
        if (len > 0) GetWindowTextW(h, &s[0], len + 1);
        return s;
    }

    bool LooksLikeUrl(const std::wstring& text) {
        if (text.find(L' ') != std::wstring::npos) return false;
        return text.find(L'.') != std::wstring::npos || text.find(L"://") != std::wstring::npos
            || text.rfind(L"localhost", 0) == 0;
    }
}

// ---------------------------------------------------------------------------
// Settings dialog - a second top-level window built the same way as the
// tool dialogs (PostmanDialog etc.), kept in this file since it needs
// direct access to BlackholeApp's settings_/adBlocker_/environment_.
// Mirrors SettingsActivity.kt + the Settings screen it describes in
// README.md ("Security Mode", "Proxy Settings", "Browser Mode",
// "Download Retention").
// ---------------------------------------------------------------------------
namespace {
    struct SettingsCtrls {
        BlackholeApp* app;
        HWND modeCombo, secCombo, proxyEnable, proxyScheme, proxyHost, proxyPort, burpBtn, retention, saveBtn;
    };
    SettingsCtrls* GetSettingsCtrls(HWND h) { return reinterpret_cast<SettingsCtrls*>(GetWindowLongPtr(h, GWLP_USERDATA)); }

    LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
}

bool BlackholeApp::Initialize(HINSTANCE hInstance, int nCmdShow) {
    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_STANDARD_CLASSES | ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    settings_ = AppSettings::Load();

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath);
    exeDir_ = exePath;

    adBlocker_.LoadFromFile(exeDir_ + kAdblockRelPath);
    downloadManager_.StartRetentionSweeper(settings_.downloadRetentionHours);

    WNDCLASSW wc{};
    wc.lpfnWndProc   = WndProcStatic;
    wc.hInstance     = hInstance;
    wc.lpszClassName = kWindowClass;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    hwndMain_ = CreateWindowExW(0, kWindowClass, L"Blackhole", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1100, 760, nullptr, nullptr, hInstance, this);
    if (!hwndMain_) return false;

    CreateToolbar(hwndMain_);
    UpdateModeIndicator();
    ShowWindow(hwndMain_, nCmdShow);
    UpdateWindow(hwndMain_);

    CreateWebViewEnvironment(); // asynchronously adds the first tab once ready
    return true;
}

int BlackholeApp::RunMessageLoop() {
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK BlackholeApp::WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    BlackholeApp* self;
    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<BlackholeApp*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)self);
    } else {
        self = reinterpret_cast<BlackholeApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    if (self) return self->WndProc(hwnd, msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void BlackholeApp::CreateToolbar(HWND parent) {
    HINSTANCE hi = GetModuleHandleW(nullptr);
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    auto mk = [&](const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id) {
        HWND h2 = CreateWindowW(cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h,
            parent, (HMENU)(INT_PTR)id, hi, nullptr);
        SendMessageW(h2, WM_SETFONT, (WPARAM)font, TRUE);
        return h2;
    };

    hwndBack_    = mk(L"BUTTON", L"<",  0, 8,   6, 30, 26, ID_BACK);
    hwndFwd_     = mk(L"BUTTON", L">",  0, 40,  6, 30, 26, ID_FWD);
    hwndReload_  = mk(L"BUTTON", L"\x21BB", 0, 72, 6, 30, 26, ID_RELOAD);
    hwndHome_    = mk(L"BUTTON", L"Home", 0, 104, 6, 50, 26, ID_HOME);
    hwndUrlBar_  = mk(L"EDIT", L"", WS_BORDER, 158, 6, 520, 26, ID_URLBAR);
    hwndGo_      = mk(L"BUTTON", L"Go", 0, 682, 6, 40, 26, ID_GO);
    hwndNewTab_  = mk(L"BUTTON", L"+Tab", 0, 726, 6, 50, 26, ID_NEWTAB);
    hwndCloseTab_= mk(L"BUTTON", L"Close Tab", 0, 778, 6, 70, 26, ID_CLOSETAB);
    hwndTools_   = mk(L"BUTTON", L"Tools", 0, 852, 6, 55, 26, ID_TOOLS);
    hwndSettings_= mk(L"BUTTON", L"Settings", 0, 911, 6, 65, 26, ID_SETTINGS);

    // Stage 2: per-tab JS toggle + request log viewer, both on the second
    // toolbar row so the first row doesn't need re-laying-out.
    hwndJsToggle_ = mk(L"BUTTON", L"JS: Off", 0, 330, 36, 70, 22, ID_JSTOGGLE);
    hwndReqLog_   = mk(L"BUTTON", L"Request Log", 0, 404, 36, 90, 22, ID_REQLOG);

    for (int i = 0; i < kMaxTabs; ++i) {
        hwndTabButtons_[i] = mk(L"BUTTON", L"", BS_PUSHBUTTON, 8 + i * 160, 36, 150, 22, ID_TAB_BASE + i);
        ShowWindow(hwndTabButtons_[i], SW_HIDE);
    }
    hwndModeLabel_ = mk(L"STATIC", L"", SS_RIGHT, 700, 38, 300, 18, 0);
}

void BlackholeApp::CreateWebViewEnvironment() {
    auto options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
    if (settings_.proxyEnabled) {
        std::wstring args = L"--proxy-server=" + settings_.ProxyServerSwitchValue() +
                             L" --proxy-bypass-list=127.0.0.1;localhost"; // ProxyManager.kt: local CTF targets stay direct
        options->put_AdditionalBrowserArguments(args.c_str());
    }

    std::wstring userDataFolder = exeDir_ + L"\\webview2-profile";
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, userDataFolder.c_str(), options.Get(),
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result) || !env) {
                    MessageBoxW(hwndMain_,
                        L"Failed to create the WebView2 environment. Install the "
                        L"WebView2 Runtime (usually already present on Windows 10/11) "
                        L"and relaunch Blackhole.", L"Blackhole", MB_ICONERROR);
                    return S_OK;
                }
                environment_ = env;
                AddTab(); // Basic mode starts with one tab, same as the Android app on launch
                return S_OK;
            }).Get());
    if (FAILED(hr)) {
        MessageBoxW(hwndMain_, L"CreateCoreWebView2EnvironmentWithOptions failed.", L"Blackhole", MB_ICONERROR);
    }
}

void BlackholeApp::AddTab(const std::wstring& initialUrl) {
    if ((int)tabs_.size() >= kMaxTabs) {
        MessageBoxW(hwndMain_, L"Blackhole caps out at 2 tabs by design (TabManager.kt's "
            L"hard 2-tab cap, ported as-is) - close a tab first.", L"Two-tab limit", MB_ICONINFORMATION);
        return;
    }
    if (!environment_) return;

    auto tab = std::make_unique<Tab>();
    Tab* tabPtr = tab.get();
    int index = (int)tabs_.size();
    tabs_.push_back(std::move(tab));

    environment_->CreateCoreWebView2Controller(hwndMain_,
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [this, tabPtr, index, initialUrl](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                if (FAILED(result) || !controller) return S_OK;
                tabPtr->controller = controller;
                controller->get_CoreWebView2(&tabPtr->webview);

                ComPtr<ICoreWebView2Settings> cwSettings;
                tabPtr->webview->get_Settings(&cwSettings);
                cwSettings->put_IsScriptEnabled(FALSE); // Settings.kt: JS off by default, per tab
                cwSettings->put_AreDefaultContextMenusEnabled(TRUE);
                tabPtr->webviewSettings = cwSettings; // Stage 2: cached for ToggleJsForActiveTab()
                // Note on file:// access: WebView2 has no per-instance toggle
                // equivalent to Android WebView's setAllowFileAccess(false)
                // (SecureWebView.kt). It's disabled for content loaded from
                // http/https origins by default; see docs/PORTING_NOTES.md.

                tabPtr->webview->add_NavigationStarting(
                    Callback<ICoreWebView2NavigationStartingEventHandler>(
                        [this, index](ICoreWebView2* wv, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                            // SecureWebView.kt's Strict/CTF scheme check, ported directly:
                            // Strict blocks http:// navigation outright; CTF allows it.
                            wchar_t* uri = nullptr;
                            args->get_Uri(&uri);
                            BOOL cancel = FALSE;
                            if (settings_.securityMode == SecurityMode::Strict && uri &&
                                wcsncmp(uri, L"http://", 7) == 0) {
                                cancel = TRUE;
                            }
                            args->put_Cancel(cancel);
                            if (cancel) {
                                MessageBeep(MB_ICONWARNING);
                            } else if (index == activeTab_ && uri) {
                                SetWindowTextW(hwndUrlBar_, uri);
                            }
                            if (uri) CoTaskMemFree(uri);
                            return S_OK;
                        }).Get(), &tabPtr->navStartToken);

                tabPtr->webview->add_NavigationCompleted(
                    Callback<ICoreWebView2NavigationCompletedEventHandler>(
                        [this, index](ICoreWebView2* wv, ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT {
                            if (index == activeTab_) {
                                wchar_t* uri = nullptr;
                                wv->get_Source(&uri);
                                if (uri) { SetWindowTextW(hwndUrlBar_, uri); CoTaskMemFree(uri); }
                            }
                            return S_OK;
                        }).Get(), &tabPtr->navCompletedToken);

                // target="_blank" / window.open(): we hard-cap at 2 tabs, so
                // route it through the same Navigate() logic as a new tab
                // request rather than letting WebView2 spawn a popup window.
                tabPtr->webview->add_NewWindowRequested(
                    Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                        [this](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                            wchar_t* uri = nullptr;
                            args->get_Uri(&uri);
                            args->put_Handled(TRUE);
                            if (uri) {
                                if ((int)tabs_.size() < kMaxTabs) AddTab(uri);
                                else Navigate(uri);
                                CoTaskMemFree(uri);
                            }
                            return S_OK;
                        }).Get(), &tabPtr->newWindowToken);

                HookAdBlocking(*tabPtr);
                HookDownloads(*tabPtr); // Stage 2

                RECT rc; GetClientRect(hwndMain_, &rc);
                rc.top = 64;
                tabPtr->controller->put_Bounds(rc);

                std::wstring startUrl = !initialUrl.empty() ? initialUrl
                    : (L"file:///" + exeDir_ + kHomepageRelPath);
                for (auto& c : startUrl) if (c == L'\\') c = L'/';
                tabPtr->webview->Navigate(startUrl.c_str());

                SwitchToTab(index);
                SetWindowTextW(hwndTabButtons_[index], L"Tab");
                ShowWindow(hwndTabButtons_[index], SW_SHOW);
                return S_OK;
            }).Get());
}

void BlackholeApp::HookAdBlocking(Tab& tab) {
    tab.webview->AddWebResourceRequestedFilter(L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    tab.webview->add_WebResourceRequested(
        Callback<ICoreWebView2WebResourceRequestedEventHandler>(
            [this, &tab](ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {
                ComPtr<ICoreWebView2WebResourceRequest> req;
                args->get_Request(&req);
                wchar_t* uriRaw = nullptr;
                req->get_Uri(&uriRaw);
                std::wstring uri = uriRaw ? uriRaw : L"";
                if (uriRaw) CoTaskMemFree(uriRaw);

                bool blocked = false;
                // Lightweight host extraction (avoids pulling in winhttp here
                // just for URL parsing): take the substring between "://"
                // and the next '/', ':' or end of string.
                size_t schemeEnd = uri.find(L"://");
                if (schemeEnd != std::wstring::npos) {
                    size_t hostStart = schemeEnd + 3;
                    size_t hostEnd = uri.find_first_of(L"/:?", hostStart);
                    std::wstring h = uri.substr(hostStart, hostEnd == std::wstring::npos ? std::wstring::npos : hostEnd - hostStart);
                    blocked = adBlocker_.IsBlocked(h);
                }

                // Stage 5: timestamp prefix added so RequestTimelineDialog
                // has something chronological to show - Stage 1-4 only
                // needed BLOCKED/allowed + URL for the plain log viewer.
                SYSTEMTIME st; GetLocalTime(&st);
                wchar_t ts[16];
                wsprintfW(ts, L"%02d:%02d:%02d.%03d ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
                tab.requestLog = ts + (blocked ? L"BLOCKED  " : L"allowed  ") + uri + L"\r\n" + tab.requestLog;
                if (++tab.requestLogCount > 200) {
                    // RequestLogEntry.kt caps at 200 entries per tab, newest
                    // first; drop everything after the 200th "\r\n".
                    size_t pos = 0;
                    for (int i = 0; i < 200 && pos != std::wstring::npos; ++i)
                        pos = tab.requestLog.find(L"\r\n", pos) + 2;
                    if (pos != std::wstring::npos && pos < tab.requestLog.size())
                        tab.requestLog.resize(pos);
                    tab.requestLogCount = 200;
                }

                if (blocked) {
                    ComPtr<ICoreWebView2WebResourceResponse> response;
                    environment_->CreateWebResourceResponse(nullptr, 403, L"Blocked by Blackhole ad blocker",
                        L"", &response);
                    args->put_Response(response.Get());
                }
                return S_OK;
            }).Get(), &tab.resourceReqToken);
}

void BlackholeApp::HookDownloads(Tab& tab) {
    // Stage 2: forces every download into DownloadManager's folder so the
    // retention sweeper actually has something to clean up (Stage 1 had
    // the folder + sweeper ready but nothing fed it). Mirrors
    // DownloadManager.kt landing files in a single app-owned directory
    // rather than the OS-wide Downloads folder.
    tab.webview->add_DownloadStarting(
        Callback<ICoreWebView2DownloadStartingEventHandler>(
            [this](ICoreWebView2*, ICoreWebView2DownloadStartingEventArgs* args) -> HRESULT {
                ComPtr<ICoreWebView2DownloadOperation> op;
                args->get_DownloadOperation(&op);

                std::wstring suggested = L"download";
                if (op) {
                    wchar_t* uri = nullptr;
                    op->get_Uri(&uri);
                    if (uri) {
                        std::wstring u = uri;
                        size_t slash = u.find_last_of(L'/');
                        size_t nameStart = (slash == std::wstring::npos) ? 0 : slash + 1;
                        size_t cut = u.find_first_of(L"?#", nameStart);
                        std::wstring candidate = u.substr(nameStart,
                            cut == std::wstring::npos ? std::wstring::npos : cut - nameStart);
                        if (!candidate.empty()) suggested = candidate;
                        CoTaskMemFree(uri);
                    }
                }

                std::wstring path = downloadManager_.ResultPathFor(suggested);
                args->put_ResultFilePath(path.c_str());
                // Skip WebView2's own download flyout - DownloadManager's
                // retention sweeper is the app's download UI equivalent,
                // same as the Android version not using the system
                // DownloadManager's notification UI either.
                args->put_Handled(TRUE);
                return S_OK;
            }).Get(), &tab.downloadToken);
}

void BlackholeApp::ApplySecurityModeEnforcement(ICoreWebView2*, int) {
    // Intentionally empty - see the inline comment in the NavigationStarting
    // handler above for where the Strict/CTF scheme check actually lives.
    // Kept as a named hook (matching SecureWebView.kt's structure) for where
    // TLS-cert-error handling would go - except WebView2 always shows its
    // own certificate-error interstitial and gives the app no click-through
    // API, so there is nothing to wire up: the "never changes between
    // Strict/CTF modes" guarantee from the Kotlin version holds here too,
    // just enforced by the platform instead of app code.
}

void BlackholeApp::OnSettingsSaved() {
    settings_.Save();
    UpdateModeIndicator();
    downloadManager_.Stop();
    downloadManager_.StartRetentionSweeper(settings_.downloadRetentionHours);
    // Proxy/browser-arguments changes only take effect for a freshly created
    // WebView2 environment (same restart requirement Chromium itself has
    // for --proxy-server), so we don't try to hot-swap environment_ here -
    // ProxyManager.kt's live toggle doesn't have a direct WebView2 analog.
    MessageBoxW(hwndMain_, L"Settings saved. Restart Blackhole for proxy changes to take effect.",
        L"Blackhole", MB_ICONINFORMATION);
}

void BlackholeApp::SwitchToTab(int index) {
    if (index < 0 || index >= (int)tabs_.size()) return;
    for (int i = 0; i < (int)tabs_.size(); ++i) {
        if (tabs_[i]->controller) tabs_[i]->controller->put_IsVisible(i == index);
    }
    activeTab_ = index;
    if (tabs_[index]->webview) {
        wchar_t* uri = nullptr;
        tabs_[index]->webview->get_Source(&uri);
        if (uri) { SetWindowTextW(hwndUrlBar_, uri); CoTaskMemFree(uri); }
    }
    UpdateJsToggleButtonText(); // Stage 2: button must reflect *this* tab's state, not the last one's
}

void BlackholeApp::CloseTab(int index) {
    if (index < 0 || index >= (int)tabs_.size()) return;
    if (tabs_[index]->controller) tabs_[index]->controller->Close();
    tabs_.erase(tabs_.begin() + index);
    for (int i = 0; i < kMaxTabs; ++i) ShowWindow(hwndTabButtons_[i], i < (int)tabs_.size() ? SW_SHOW : SW_HIDE);
    if (!tabs_.empty()) SwitchToTab(0); else activeTab_ = -1;
}

void BlackholeApp::LayoutControls() {
    if (!hwndMain_) return;
    RECT rc; GetClientRect(hwndMain_, &rc);
    rc.top = 64; // below the toolbar + tab-pill rows
    // Resize every instantiated tab's controller, not just the active one,
    // so switching tabs after a resize doesn't show a stale size for a frame.
    for (auto& t : tabs_) {
        if (!t->controller) continue;
        if (t->viewportOverride) {
            // Stage 5 (ViewportEmulatorDialog): shrink the controller to
            // the chosen device size, pinned to the top-left corner of the
            // content area - a crude but functional viewport emulator
            // (no scaling/centering chrome like Chrome DevTools draws).
            RECT vp = rc;
            vp.right  = std::min(rc.right,  rc.left + t->viewportW);
            vp.bottom = std::min(rc.bottom, rc.top + t->viewportH);
            t->controller->put_Bounds(vp);
        } else {
            t->controller->put_Bounds(rc);
        }
    }
}

void BlackholeApp::Navigate(const std::wstring& urlOrQuery) {
    if (activeTab_ < 0 || !tabs_[activeTab_]->webview) return;
    std::wstring target = urlOrQuery;
    if (!LooksLikeUrl(target)) {
        std::wstring q = target;
        std::wstring encoded;
        wchar_t buf[8];
        for (wchar_t c : q) {
            if (iswalnum(c)) encoded += c;
            else { wsprintfW(buf, L"%%%02X", c); encoded += buf; }
        }
        target = L"https://duckduckgo.com/?q=" + encoded;
    } else if (target.find(L"://") == std::wstring::npos) {
        // README: CTF mode defaults a bare host/IP to http://, Strict defaults to https://.
        target = (settings_.securityMode == SecurityMode::Ctf ? L"http://" : L"https://") + target;
    }
    tabs_[activeTab_]->webview->Navigate(target.c_str());
}

void BlackholeApp::UpdateModeIndicator() {
    if (!hwndModeLabel_) return;
    std::wstring text = std::wstring(L"Mode: ") + BrowserModeName(settings_.browserMode) +
                         L"   |   " + SecurityModeName(settings_.securityMode);
    if (settings_.proxyEnabled) text += L"   |   Proxy: " + settings_.ProxyServerSwitchValue();
    SetWindowTextW(hwndModeLabel_, text.c_str());
}

void BlackholeApp::ExecuteScriptOnActiveTab(const std::wstring& script,
                                            std::function<void(const std::wstring&)> callback) {
    if (activeTab_ < 0 || !tabs_[activeTab_]->webview) { callback(L"(no active tab)"); return; }
    // Advance mode: force JS on for this call so the console works even if
    // the per-tab toggle is off, same convenience the Kotlin console had.
    tabs_[activeTab_]->webview->ExecuteScript(script.c_str(),
        Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
            [callback](HRESULT, LPCWSTR resultJson) -> HRESULT {
                callback(resultJson ? resultJson : L"(no result)");
                return S_OK;
            }).Get());
}

void BlackholeApp::ToggleJsForActiveTab() {
    if (activeTab_ < 0 || !tabs_[activeTab_]->webviewSettings) return;
    Tab& t = *tabs_[activeTab_];
    t.jsEnabled = !t.jsEnabled;
    t.webviewSettings->put_IsScriptEnabled(t.jsEnabled ? TRUE : FALSE);
    UpdateJsToggleButtonText();
}

void BlackholeApp::UpdateJsToggleButtonText() {
    if (!hwndJsToggle_) return;
    bool on = (activeTab_ >= 0 && activeTab_ < (int)tabs_.size() && tabs_[activeTab_]->jsEnabled);
    SetWindowTextW(hwndJsToggle_, on ? L"JS: On" : L"JS: Off");
}

void BlackholeApp::ShowRequestLogForActiveTab() {
    if (activeTab_ < 0 || activeTab_ >= (int)tabs_.size()) {
        MessageBoxW(hwndMain_, L"No active tab.", L"Blackhole", MB_ICONINFORMATION);
        return;
    }
    // Captures the raw Tab* (owned by tabs_, stable for the tab's lifetime).
    // If the tab is closed while this viewer is open, Refresh will read a
    // dangling pointer - acceptable for a first pass, noted in
    // docs/PORTING_NOTES.md as a follow-up (close the viewer alongside its tab).
    Tab* t = tabs_[activeTab_].get();
    RequestLogDialog::ShowFor(hwndMain_, [t]() { return t->requestLog; });
}

void BlackholeApp::SetUserAgentOnActiveTab(const std::wstring& ua) {
    if (activeTab_ < 0 || activeTab_ >= (int)tabs_.size() || !tabs_[activeTab_]->webviewSettings) return;

    // put_UserAgent lives on ICoreWebView2Settings2, not the base
    // ICoreWebView2Settings this app otherwise uses - QI for it here so
    // only this one call site needs to know about the versioned interface.
    ComPtr<ICoreWebView2Settings2> settings2;
    if (SUCCEEDED(tabs_[activeTab_]->webviewSettings.As(&settings2))) {
        settings2->put_UserAgent(ua.c_str());
        if (tabs_[activeTab_]->webview) tabs_[activeTab_]->webview->Reload();
    } else {
        MessageBoxW(hwndMain_,
            L"This WebView2 runtime doesn't expose ICoreWebView2Settings2 - "
            L"the user-agent override couldn't be applied.",
            L"Blackhole", MB_ICONWARNING);
    }
}

void BlackholeApp::GetCookiesForActiveTab(std::function<void(const std::wstring&)> callback) {
    if (activeTab_ < 0 || activeTab_ >= (int)tabs_.size() || !tabs_[activeTab_]->webview) {
        callback(L"(no active tab)");
        return;
    }
    ComPtr<ICoreWebView2_2> wv2;
    if (FAILED(tabs_[activeTab_]->webview.As(&wv2)) || !wv2) {
        callback(L"(ICoreWebView2_2 / cookie manager unavailable on this WebView2 runtime)");
        return;
    }
    ComPtr<ICoreWebView2CookieManager> mgr;
    wv2->get_CookieManager(&mgr);
    if (!mgr) { callback(L"(no cookie manager)"); return; }

    wchar_t* uri = nullptr;
    tabs_[activeTab_]->webview->get_Source(&uri);
    std::wstring currentUri = uri ? uri : L"";
    if (uri) CoTaskMemFree(uri);

    mgr->GetCookies(currentUri.c_str(),
        Callback<ICoreWebView2GetCookiesCompletedHandler>(
            [callback](HRESULT, ICoreWebView2CookieList* list) -> HRESULT {
                UINT count = 0;
                if (list) list->get_Count(&count);
                std::wstringstream out;
                out << count << L" cookie(s):\r\n\r\n";
                for (UINT i = 0; i < count; ++i) {
                    ComPtr<ICoreWebView2Cookie> cookie;
                    list->GetValueAtIndex(i, &cookie);
                    if (!cookie) continue;
                    LPWSTR name = nullptr, value = nullptr, domain = nullptr, path = nullptr;
                    cookie->get_Name(&name);
                    cookie->get_Value(&value);
                    cookie->get_Domain(&domain);
                    cookie->get_Path(&path);
                    BOOL httpOnly = FALSE, secure = FALSE;
                    cookie->get_IsHttpOnly(&httpOnly);
                    cookie->get_IsSecure(&secure);
                    out << (name ? name : L"") << L" = " << (value ? value : L"") << L"\r\n"
                        << L"    domain=" << (domain ? domain : L"") << L"  path=" << (path ? path : L"")
                        << L"  httpOnly=" << (httpOnly ? L"yes" : L"no")
                        << L"  secure=" << (secure ? L"yes" : L"no") << L"\r\n\r\n";
                    if (name) CoTaskMemFree(name);
                    if (value) CoTaskMemFree(value);
                    if (domain) CoTaskMemFree(domain);
                    if (path) CoTaskMemFree(path);
                }
                callback(out.str());
                return S_OK;
            }).Get());
}

void BlackholeApp::CaptureScreenshotOfActiveTab(const std::wstring& pngPath,
                                                 std::function<void(bool, const std::wstring&)> callback) {
    if (activeTab_ < 0 || activeTab_ >= (int)tabs_.size() || !tabs_[activeTab_]->webview) {
        callback(false, L"No active tab.");
        return;
    }
    ComPtr<IStream> stream;
    HRESULT hr = SHCreateStreamOnFileEx(pngPath.c_str(),
        STGM_CREATE | STGM_WRITE | STGM_SHARE_EXCLUSIVE, FILE_ATTRIBUTE_NORMAL, TRUE, nullptr, &stream);
    if (FAILED(hr) || !stream) {
        callback(false, L"Could not create output file at " + pngPath);
        return;
    }

    tabs_[activeTab_]->webview->CapturePreview(COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT_PNG, stream.Get(),
        Callback<ICoreWebView2CapturePreviewCompletedHandler>(
            [callback, pngPath](HRESULT result) -> HRESULT {
                if (SUCCEEDED(result)) callback(true, pngPath);
                else callback(false, L"CapturePreview failed (HRESULT 0x" + std::to_wstring((unsigned)result) + L").");
                return S_OK;
            }).Get());
}

void BlackholeApp::SetViewportOverrideForActiveTab(int width, int height) {
    if (activeTab_ < 0 || activeTab_ >= (int)tabs_.size()) return;
    Tab& t = *tabs_[activeTab_];
    t.viewportOverride = true;
    t.viewportW = width;
    t.viewportH = height;
    LayoutControls();
}

void BlackholeApp::ClearViewportOverrideForActiveTab() {
    if (activeTab_ < 0 || activeTab_ >= (int)tabs_.size()) return;
    tabs_[activeTab_]->viewportOverride = false;
    LayoutControls();
}

std::wstring BlackholeApp::GetRequestLogForActiveTab() {
    if (activeTab_ < 0 || activeTab_ >= (int)tabs_.size()) return L"";
    return tabs_[activeTab_]->requestLog;
}

void BlackholeApp::OpenToolForCurrentMode(int toolMenuId) {
    switch (toolMenuId) {
        case ID_TOOL_HTTP:
            if (settings_.browserMode < BrowserMode::Intermediate) {
                MessageBoxW(hwndMain_, L"The HTTP Request Tool unlocks in Intermediate mode or higher. "
                    L"Switch modes in Settings.", L"Blackhole", MB_ICONINFORMATION);
                return;
            }
            PostmanDialog::ShowFor(hwndMain_);
            return;
        case ID_TOOL_JSCONSOLE:
            if (settings_.browserMode < BrowserMode::Advance) {
                MessageBoxW(hwndMain_, L"The JavaScript Console unlocks in Advance mode or higher. "
                    L"Switch modes in Settings.", L"Blackhole", MB_ICONINFORMATION);
                return;
            }
            JsConsoleDialog::ShowFor(hwndMain_, [this](const std::wstring& script, auto cb) {
                ExecuteScriptOnActiveTab(script, cb);
            });
            return;
        case ID_TOOL_DATATOOLKIT:
            DataToolkitDialog::ShowFor(hwndMain_);
            return;
        case ID_TOOL_JSON:
            JsonFormatterDialog::ShowFor(hwndMain_);
            return;
        case ID_TOOL_JWT:
            JwtDecoderDialog::ShowFor(hwndMain_);
            return;
        case ID_TOOL_REGEX:
            RegexTesterDialog::ShowFor(hwndMain_);
            return;
        case ID_TOOL_HASHID:
            HashIdentifierDialog::ShowFor(hwndMain_);
            return;
        case ID_TOOL_TOTP:
            TotpGeneratorDialog::ShowFor(hwndMain_);
            return;
        case ID_TOOL_USERAGENT:
            UserAgentSwitcherDialog::ShowFor(hwndMain_, [this](const std::wstring& ua) {
                SetUserAgentOnActiveTab(ua);
            });
            return;
        case ID_TOOL_DNS:
            DnsLookupDialog::ShowFor(hwndMain_);
            return;
        case ID_TOOL_WHOIS:
            WhoisLookupDialog::ShowFor(hwndMain_);
            return;
        case ID_TOOL_TLS:
            TlsInspectorDialog::ShowFor(hwndMain_);
            return;
        case ID_TOOL_SECHEADERS:
            SecurityHeaderScannerDialog::ShowFor(hwndMain_);
            return;
        case ID_TOOL_TECHFP:
            TechFingerprintDialog::ShowFor(hwndMain_);
            return;
        case ID_TOOL_ROBOTS:
            RobotsFetchDialog::ShowFor(hwndMain_);
            return;
        case ID_TOOL_FAVICON:
            FaviconHashDialog::ShowFor(hwndMain_);
            return;
        case ID_TOOL_A11Y:
            AccessibilityCheckerDialog::ShowFor(hwndMain_, [this](const std::wstring& script, auto cb) {
                ExecuteScriptOnActiveTab(script, cb);
            });
            return;
        case ID_TOOL_BOOKMARKLET:
            BookmarkletRunnerDialog::ShowFor(hwndMain_, [this](const std::wstring& script, auto cb) {
                ExecuteScriptOnActiveTab(script, cb);
            });
            return;
        case ID_TOOL_CASEFILE:
            CaseFileExportDialog::ShowFor(hwndMain_);
            return;
        case ID_TOOL_CIPHER:
            CipherSolverDialog::ShowFor(hwndMain_);
            return;
        case ID_TOOL_COOKIES:
            CookieInspectorDialog::ShowFor(hwndMain_, [this](auto cb) {
                GetCookiesForActiveTab(cb);
            });
            return;
        case ID_TOOL_DIFF:
            DiffViewerDialog::ShowFor(hwndMain_);
            return;
        case ID_TOOL_FILEINSPECT:
            FileInspectorDialog::ShowFor(hwndMain_);
            return;
        case ID_TOOL_GRAPHQL:
            GraphQLIntrospectionDialog::ShowFor(hwndMain_);
            return;
        case ID_TOOL_HAR:
            HarExportDialog::ShowFor(hwndMain_, [this]() { return GetRequestLogForActiveTab(); });
            return;
        case ID_TOOL_TIMELINE:
            RequestTimelineDialog::ShowFor(hwndMain_, [this]() { return GetRequestLogForActiveTab(); });
            return;
        case ID_TOOL_SCREENSHOT:
            ScreenshotCaptureDialog::ShowFor(hwndMain_, [this](const std::wstring& path, auto cb) {
                CaptureScreenshotOfActiveTab(path, cb);
            });
            return;
        case ID_TOOL_STORAGE:
            StorageInspectorDialog::ShowFor(hwndMain_, [this](const std::wstring& script, auto cb) {
                ExecuteScriptOnActiveTab(script, cb);
            });
            return;
        case ID_TOOL_STRINGS:
            StringsExtractorDialog::ShowFor(hwndMain_);
            return;
        case ID_TOOL_VIEWPORT:
            ViewportEmulatorDialog::ShowFor(hwndMain_,
                [this](int w, int h) { SetViewportOverrideForActiveTab(w, h); },
                [this]() { ClearViewportOverrideForActiveTab(); });
            return;
        case ID_TOOL_EXPLOITSHELL:
            if (settings_.browserMode < BrowserMode::ByteBandit) {
                MessageBoxW(hwndMain_, L"The Exploit Shell unlocks in ByteBandit mode. "
                    L"Switch modes in Settings.", L"Blackhole", MB_ICONINFORMATION);
                return;
            }
            ExploitShellDialog::ShowFor(hwndMain_);
            return;
        case ID_TOOL_SSH_STUB:
            if (settings_.browserMode < BrowserMode::ByteBandit) {
                MessageBoxW(hwndMain_, L"SSH Terminal unlocks in ByteBandit mode. "
                    L"Switch modes in Settings.", L"Blackhole", MB_ICONINFORMATION);
                return;
            }
            SSHTerminalDialog::ShowFor(hwndMain_);
            return;
        case ID_TOOL_PORTSCAN_STUB:
            if (settings_.browserMode < BrowserMode::ByteBandit) {
                MessageBoxW(hwndMain_, L"Port Scanner unlocks in ByteBandit mode. "
                    L"Switch modes in Settings.", L"Blackhole", MB_ICONINFORMATION);
                return;
            }
            PortScannerDialog::ShowFor(hwndMain_);
            return;
        case ID_TOOL_HYDRA_STUB:
            if (settings_.browserMode < BrowserMode::ByteBandit) {
                MessageBoxW(hwndMain_, L"Hydra-lite unlocks in ByteBandit mode. "
                    L"Switch modes in Settings.", L"Blackhole", MB_ICONINFORMATION);
                return;
            }
            HydraLiteDialog::ShowFor(hwndMain_);
            return;
        case ID_TOOL_DIRBUST_STUB:
            if (settings_.browserMode < BrowserMode::ByteBandit) {
                MessageBoxW(hwndMain_, L"Directory Buster unlocks in ByteBandit mode. "
                    L"Switch modes in Settings.", L"Blackhole", MB_ICONINFORMATION);
                return;
            }
            DirectoryBusterDialog::ShowFor(hwndMain_);
            return;
        case ID_TOOL_HASHCRACK:
            if (settings_.browserMode < BrowserMode::ByteBandit) {
                MessageBoxW(hwndMain_, L"Hash Cracker unlocks in ByteBandit mode. "
                    L"Switch modes in Settings.", L"Blackhole", MB_ICONINFORMATION);
                return;
            }
            HashCrackerDialog::ShowFor(hwndMain_);
            return;
    }
}

void BlackholeApp::ShowSettingsDialog() {
    static bool registered = false;
    const wchar_t* cls = L"BlackholeSettingsWindow";
    if (!registered) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = SettingsWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = cls;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        RegisterClassW(&wc);
        registered = true;
    }
    HWND hwnd = CreateWindowExW(0, cls, L"Blackhole - Settings", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 420, 360, hwndMain_, nullptr, GetModuleHandleW(nullptr), this);
    ShowWindow(hwnd, SW_SHOW);
}

LRESULT BlackholeApp::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE:
            LayoutControls();
            return 0;
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (HIWORD(wParam) != BN_CLICKED && id != ID_URLBAR) break;
            if (id == ID_GO) { Navigate(GetWindowTextStr(hwndUrlBar_)); return 0; }
            if (id == ID_BACK)   { if (activeTab_ >= 0) tabs_[activeTab_]->webview->GoBack(); return 0; }
            if (id == ID_FWD)    { if (activeTab_ >= 0) tabs_[activeTab_]->webview->GoForward(); return 0; }
            if (id == ID_RELOAD) { if (activeTab_ >= 0) tabs_[activeTab_]->webview->Reload(); return 0; }
            if (id == ID_HOME)   { std::wstring h = L"file:///" + exeDir_ + kHomepageRelPath;
                                    for (auto& c : h) if (c == L'\\') c = L'/';
                                    Navigate(h); return 0; }
            if (id == ID_NEWTAB)   { AddTab(); return 0; }
            if (id == ID_CLOSETAB) { if (activeTab_ >= 0) CloseTab(activeTab_); return 0; }
            if (id == ID_SETTINGS) { ShowSettingsDialog(); return 0; }
            if (id == ID_JSTOGGLE) { ToggleJsForActiveTab(); return 0; }
            if (id == ID_REQLOG)   { ShowRequestLogForActiveTab(); return 0; }
            if (id == ID_TOOLS) {
                HMENU menu = CreatePopupMenu();
                AppendMenuW(menu, MF_STRING, ID_TOOL_HTTP, L"HTTP Request Tool (Intermediate+)");
                AppendMenuW(menu, MF_STRING, ID_TOOL_JSCONSOLE, L"JavaScript Console (Advance+)");
                AppendMenuW(menu, MF_STRING, ID_TOOL_DATATOOLKIT, L"Data Toolkit (Base64 / hashes)");
                AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(menu, MF_STRING, ID_TOOL_JSON, L"JSON Formatter");
                AppendMenuW(menu, MF_STRING, ID_TOOL_JWT, L"JWT Decoder");
                AppendMenuW(menu, MF_STRING, ID_TOOL_REGEX, L"Regex Tester");
                AppendMenuW(menu, MF_STRING, ID_TOOL_HASHID, L"Hash Identifier");
                AppendMenuW(menu, MF_STRING, ID_TOOL_TOTP, L"TOTP Generator");
                AppendMenuW(menu, MF_STRING, ID_TOOL_USERAGENT, L"User-Agent Switcher");
                AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(menu, MF_STRING, ID_TOOL_DNS, L"DNS Lookup");
                AppendMenuW(menu, MF_STRING, ID_TOOL_WHOIS, L"WHOIS Lookup");
                AppendMenuW(menu, MF_STRING, ID_TOOL_TLS, L"TLS Inspector");
                AppendMenuW(menu, MF_STRING, ID_TOOL_SECHEADERS, L"Security Header Scanner");
                AppendMenuW(menu, MF_STRING, ID_TOOL_TECHFP, L"Tech Fingerprint");
                AppendMenuW(menu, MF_STRING, ID_TOOL_ROBOTS, L"robots.txt Fetch");
                AppendMenuW(menu, MF_STRING, ID_TOOL_FAVICON, L"Favicon Hash");
                AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(menu, MF_STRING, ID_TOOL_A11Y, L"Accessibility Checker");
                AppendMenuW(menu, MF_STRING, ID_TOOL_BOOKMARKLET, L"Bookmarklet Runner");
                AppendMenuW(menu, MF_STRING, ID_TOOL_CIPHER, L"Cipher Solver");
                AppendMenuW(menu, MF_STRING, ID_TOOL_COOKIES, L"Cookie Inspector");
                AppendMenuW(menu, MF_STRING, ID_TOOL_DIFF, L"Diff Viewer");
                AppendMenuW(menu, MF_STRING, ID_TOOL_FILEINSPECT, L"File Inspector");
                AppendMenuW(menu, MF_STRING, ID_TOOL_GRAPHQL, L"GraphQL Introspection");
                AppendMenuW(menu, MF_STRING, ID_TOOL_HAR, L"Export Request Log as .har");
                AppendMenuW(menu, MF_STRING, ID_TOOL_TIMELINE, L"Request Timeline");
                AppendMenuW(menu, MF_STRING, ID_TOOL_SCREENSHOT, L"Screenshot Capture");
                AppendMenuW(menu, MF_STRING, ID_TOOL_STORAGE, L"Storage Inspector");
                AppendMenuW(menu, MF_STRING, ID_TOOL_STRINGS, L"Strings Extractor");
                AppendMenuW(menu, MF_STRING, ID_TOOL_VIEWPORT, L"Viewport Emulator");
                AppendMenuW(menu, MF_STRING, ID_TOOL_CASEFILE, L"Case File Export");
                AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(menu, MF_STRING, ID_TOOL_EXPLOITSHELL, L"Exploit Shell (ByteBandit - recon-only)");
                AppendMenuW(menu, MF_STRING, ID_TOOL_SSH_STUB, L"SSH Terminal (ByteBandit)");
                AppendMenuW(menu, MF_STRING, ID_TOOL_PORTSCAN_STUB, L"Port Scanner (ByteBandit)");
                AppendMenuW(menu, MF_STRING, ID_TOOL_HYDRA_STUB, L"Hydra-lite (ByteBandit)");
                AppendMenuW(menu, MF_STRING, ID_TOOL_DIRBUST_STUB, L"Directory Buster (ByteBandit)");
                AppendMenuW(menu, MF_STRING, ID_TOOL_HASHCRACK, L"Hash Cracker (ByteBandit)");
                RECT r; GetWindowRect(hwndTools_, &r);
                TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_TOPALIGN, r.left, r.bottom, 0, hwnd, nullptr);
                DestroyMenu(menu);
                return 0;
            }
            if (id >= ID_TOOL_HTTP && id <= ID_TOOL_DIRBUST_STUB) { OpenToolForCurrentMode(id); return 0; }
            if (id >= ID_TAB_BASE && id < ID_TAB_BASE + kMaxTabs) { SwitchToTab(id - ID_TAB_BASE); return 0; }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            downloadManager_.Stop();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Settings window implementation
// ---------------------------------------------------------------------------
namespace {
LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_NCCREATE: {
            auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            auto* app = reinterpret_cast<BlackholeApp*>(cs->lpCreateParams);
            SettingsCtrls* c = new SettingsCtrls{ app };
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)c);
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
        case WM_CREATE: {
            SettingsCtrls* c = GetSettingsCtrls(hwnd);
            HINSTANCE hi = GetModuleHandleW(nullptr);
            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            auto mk = [&](const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id) {
                HWND h2 = CreateWindowW(cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, hwnd,
                    (HMENU)(INT_PTR)id, hi, nullptr);
                SendMessageW(h2, WM_SETFONT, (WPARAM)font, TRUE);
                return h2;
            };

            mk(L"STATIC", L"Browser Mode:", 0, 10, 12, 150, 18, 0);
            c->modeCombo = mk(L"COMBOBOX", L"", CBS_DROPDOWNLIST, 170, 10, 200, 100, ID_SET_MODE);
            for (auto m : { L"Basic", L"Intermediate", L"Advance", L"ByteBandit" })
                SendMessageW(c->modeCombo, CB_ADDSTRING, 0, (LPARAM)m);

            mk(L"STATIC", L"Security Mode:", 0, 10, 44, 150, 18, 0);
            c->secCombo = mk(L"COMBOBOX", L"", CBS_DROPDOWNLIST, 170, 44, 200, 100, ID_SET_SECMODE);
            SendMessageW(c->secCombo, CB_ADDSTRING, 0, (LPARAM)L"Strict (HTTPS only)");
            SendMessageW(c->secCombo, CB_ADDSTRING, 0, (LPARAM)L"CTF (HTTP allowed)");

            c->proxyEnable = mk(L"BUTTON", L"Enable SOCKS/HTTP proxy routing", BS_AUTOCHECKBOX, 10, 80, 300, 20, ID_SET_PROXY_ENABLE);

            mk(L"STATIC", L"Scheme:", 0, 10, 108, 60, 18, 0);
            c->proxyScheme = mk(L"EDIT", L"socks5", WS_BORDER, 70, 106, 70, 22, ID_SET_PROXY_SCHEME);
            mk(L"STATIC", L"Host:", 0, 150, 108, 40, 18, 0);
            c->proxyHost = mk(L"EDIT", L"127.0.0.1", WS_BORDER, 190, 106, 110, 22, ID_SET_PROXY_HOST);
            mk(L"STATIC", L"Port:", 0, 306, 108, 36, 18, 0);
            c->proxyPort = mk(L"EDIT", L"9050", WS_BORDER, 340, 106, 50, 22, ID_SET_PROXY_PORT);

            c->burpBtn = mk(L"BUTTON", L"Burp Suite quick-fill (127.0.0.1:8080)", 0, 10, 138, 300, 24, ID_SET_BURP);

            mk(L"STATIC", L"Download retention (hours):", 0, 10, 176, 200, 18, 0);
            c->retention = mk(L"EDIT", L"24", WS_BORDER, 220, 174, 60, 22, ID_SET_RETENTION);

            mk(L"STATIC", L"Changing the proxy takes effect after restart\n(same as Chromium's --proxy-server flag).",
               0, 10, 206, 380, 36, 0);

            c->saveBtn = mk(L"BUTTON", L"Save", 0, 10, 250, 100, 30, ID_SET_SAVE);

            // Populate controls from the app's current settings.
            const AppSettings& s = c->app->SettingsRef();
            SendMessageW(c->modeCombo, CB_SETCURSEL, (int)s.browserMode, 0);
            SendMessageW(c->secCombo,  CB_SETCURSEL, (int)s.securityMode, 0);
            SendMessageW(c->proxyEnable, BM_SETCHECK, s.proxyEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
            SetWindowTextW(c->proxyScheme, s.proxyScheme.c_str());
            SetWindowTextW(c->proxyHost, s.proxyHost.c_str());
            SetWindowTextW(c->proxyPort, std::to_wstring(s.proxyPort).c_str());
            SetWindowTextW(c->retention, std::to_wstring(s.downloadRetentionHours).c_str());
            return 0;
        }
        case WM_COMMAND: {
            SettingsCtrls* c = GetSettingsCtrls(hwnd);
            if (LOWORD(wParam) == ID_SET_BURP && HIWORD(wParam) == BN_CLICKED) {
                SetWindowTextW(c->proxyScheme, L"http");
                SetWindowTextW(c->proxyHost, L"127.0.0.1");
                SetWindowTextW(c->proxyPort, L"8080");
                SendMessageW(c->proxyEnable, BM_SETCHECK, BST_CHECKED, 0);
                return 0;
            }
            if (LOWORD(wParam) == ID_SET_SAVE && HIWORD(wParam) == BN_CLICKED) {
                AppSettings& s = c->app->SettingsRef();
                s.browserMode  = (BrowserMode)SendMessageW(c->modeCombo, CB_GETCURSEL, 0, 0);
                s.securityMode = (SecurityMode)SendMessageW(c->secCombo, CB_GETCURSEL, 0, 0);
                s.proxyEnabled = SendMessageW(c->proxyEnable, BM_GETCHECK, 0, 0) == BST_CHECKED;

                auto getText = [](HWND h) {
                    int len = GetWindowTextLengthW(h);
                    std::wstring t(len, L'\0');
                    if (len > 0) GetWindowTextW(h, &t[0], len + 1);
                    return t;
                };
                s.proxyScheme = getText(c->proxyScheme);
                s.proxyHost   = getText(c->proxyHost);
                s.proxyPort   = _wtoi(getText(c->proxyPort).c_str());
                s.downloadRetentionHours = _wtoi(getText(c->retention).c_str());

                c->app->OnSettingsSaved();
                DestroyWindow(hwnd);
                return 0;
            }
            return 0;
        }
        case WM_DESTROY:
            delete GetSettingsCtrls(hwnd);
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
} // namespace
