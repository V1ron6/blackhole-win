#include "UserAgentSwitcherDialog.h"

namespace {
    constexpr int ID_PRESETS = 1001, ID_CUSTOM = 1002, ID_APPLY = 1003, ID_RESET = 1004;
    const wchar_t* kClassName = L"BlackholeUserAgentSwitcher";

    struct PresetEntry { const wchar_t* label; const wchar_t* ua; };
    // Same preset set UserAgentSwitcher.kt shipped: a representative
    // desktop, mobile, and bot UA for quick site-rendering tests.
    const PresetEntry kPresets[] = {
        { L"Windows / Chrome",
          L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36" },
        { L"Android / Chrome",
          L"Mozilla/5.0 (Linux; Android 14; Pixel 8) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Mobile Safari/537.36" },
        { L"iPhone / Safari",
          L"Mozilla/5.0 (iPhone; CPU iPhone OS 17_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.0 Mobile/15E148 Safari/604.1" },
        { L"Googlebot",
          L"Mozilla/5.0 (compatible; Googlebot/2.1; +http://www.google.com/bot.html)" },
    };

    struct Controls {
        HWND presets, custom;
        std::function<void(const std::wstring&)> applyFn;
        std::wstring defaultUa; // WebView2's own default, for Reset
    };
    Controls* GetControls(HWND h) { return reinterpret_cast<Controls*>(GetWindowLongPtr(h, GWLP_USERDATA)); }

    std::wstring GetText(HWND h) {
        int len = GetWindowTextLengthW(h);
        std::wstring s(len, L'\0');
        if (len > 0) GetWindowTextW(h, &s[0], len + 1);
        return s;
    }
}

void UserAgentSwitcherDialog::RegisterClassOnce() {
    static bool registered = false;
    if (registered) return;
    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);
    registered = true;
}

void UserAgentSwitcherDialog::ShowFor(HWND owner, std::function<void(const std::wstring&)> applyFn) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - User-Agent Switcher (active tab)",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 520, 260,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    Controls* c = GetControls(hwnd);
    if (c) c->applyFn = std::move(applyFn);
    ShowWindow(hwnd, SW_SHOW);
}

LRESULT CALLBACK UserAgentSwitcherDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            Controls* c = new Controls();
            CreateWindowW(L"STATIC", L"Preset:", WS_CHILD | WS_VISIBLE, 10, 10, 100, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            c->presets = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                10, 30, 480, 200, hwnd, (HMENU)(INT_PTR)ID_PRESETS, GetModuleHandleW(nullptr), nullptr);
            for (auto& p : kPresets) SendMessageW(c->presets, CB_ADDSTRING, 0, (LPARAM)p.label);
            SendMessageW(c->presets, CB_ADDSTRING, 0, (LPARAM)L"Custom...");

            CreateWindowW(L"STATIC", L"User-Agent string:", WS_CHILD | WS_VISIBLE, 10, 64, 200, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            c->custom = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE,
                10, 84, 480, 70, hwnd, (HMENU)(INT_PTR)ID_CUSTOM, GetModuleHandleW(nullptr), nullptr);

            CreateWindowW(L"BUTTON", L"Apply to active tab", WS_CHILD | WS_VISIBLE, 10, 164, 160, 28,
                hwnd, (HMENU)(INT_PTR)ID_APPLY, GetModuleHandleW(nullptr), nullptr);
            CreateWindowW(L"STATIC",
                L"Reloads the active tab with the new UA. Resets to default on app restart.",
                WS_CHILD | WS_VISIBLE, 10, 198, 480, 18, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            SendMessageW(c->presets, WM_SETFONT, (WPARAM)font, TRUE);
            SendMessageW(c->custom, WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)c);
            return 0;
        }
        case WM_COMMAND: {
            Controls* c = GetControls(hwnd);
            if (LOWORD(wParam) == ID_PRESETS && HIWORD(wParam) == CBN_SELCHANGE) {
                int sel = (int)SendMessageW(c->presets, CB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < (int)(sizeof(kPresets) / sizeof(kPresets[0]))) {
                    SetWindowTextW(c->custom, kPresets[sel].ua);
                } else {
                    SetWindowTextW(c->custom, L"");
                }
                return 0;
            }
            if (LOWORD(wParam) == ID_APPLY && HIWORD(wParam) == BN_CLICKED) {
                std::wstring ua = GetText(c->custom);
                if (!ua.empty() && c->applyFn) c->applyFn(ua);
                return 0;
            }
            return 0;
        }
        case WM_DESTROY:
            delete GetControls(hwnd);
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
