#include "CookieInspectorDialog.h"

namespace {
    constexpr int ID_REFRESH = 2101, ID_RESULTS = 2102;
    const wchar_t* kClassName = L"BlackholeCookieInspector";

    struct State {
        HWND refresh, results;
        CookieInspectorDialog::GetCookiesFn getCookies;
    };
    State* GetState(HWND h) { return reinterpret_cast<State*>(GetWindowLongPtr(h, GWLP_USERDATA)); }
}

void CookieInspectorDialog::RegisterClassOnce() {
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

void CookieInspectorDialog::ShowFor(HWND owner, GetCookiesFn getCookies) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - Cookie Inspector (active tab, read-only)",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 540, 440,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    State* st = GetState(hwnd);
    if (st) {
        st->getCookies = std::move(getCookies);
        if (st->getCookies) {
            SetWindowTextW(st->results, L"Loading...");
            st->getCookies([st](const std::wstring& text) { SetWindowTextW(st->results, text.c_str()); });
        }
    }
    ShowWindow(hwnd, SW_SHOW);
}

LRESULT CALLBACK CookieInspectorDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            State* st = new State();
            st->refresh = CreateWindowW(L"BUTTON", L"Refresh", WS_CHILD | WS_VISIBLE,
                10, 10, 100, 26, hwnd, (HMENU)(INT_PTR)ID_REFRESH, GetModuleHandleW(nullptr), nullptr);
            st->results = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_HSCROLL, 10, 46, 500, 350,
                hwnd, (HMENU)(INT_PTR)ID_RESULTS, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            SendMessageW(st->refresh, WM_SETFONT, (WPARAM)font, TRUE);
            SendMessageW(st->results, WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)st);
            return 0;
        }
        case WM_COMMAND: {
            State* st = GetState(hwnd);
            if (LOWORD(wParam) == ID_REFRESH && HIWORD(wParam) == BN_CLICKED && st->getCookies) {
                SetWindowTextW(st->results, L"Loading...");
                st->getCookies([st](const std::wstring& text) { SetWindowTextW(st->results, text.c_str()); });
            }
            return 0;
        }
        case WM_DESTROY:
            delete GetState(hwnd);
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
