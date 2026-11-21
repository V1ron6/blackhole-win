#include "RequestLogDialog.h"

namespace {
    constexpr int ID_LOG_TEXT = 401;
    constexpr int ID_LOG_REFRESH = 402;
    const wchar_t* kClassName = L"BlackholeRequestLog";

    struct State {
        HWND text, refresh;
        std::function<std::wstring()> getLatest;
    };
    State* GetState(HWND h) { return reinterpret_cast<State*>(GetWindowLongPtr(h, GWLP_USERDATA)); }
}

void RequestLogDialog::RegisterClassOnce() {
    static bool registered = false;
    if (registered) return;
    WNDCLASSW wc{};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = kClassName;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);
    registered = true;
}

void RequestLogDialog::ShowFor(HWND owner, std::function<std::wstring()> getLatest) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - Per-Tab Request Log",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 620, 480,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    State* st = GetState(hwnd);
    if (st) {
        st->getLatest = std::move(getLatest);
        if (st->getLatest) SetWindowTextW(st->text, st->getLatest().c_str());
    }
    ShowWindow(hwnd, SW_SHOW);
}

LRESULT CALLBACK RequestLogDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            State* st = new State();
            st->text = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_HSCROLL,
                10, 10, 580, 390, hwnd, (HMENU)(INT_PTR)ID_LOG_TEXT, GetModuleHandleW(nullptr), nullptr);
            st->refresh = CreateWindowW(L"BUTTON", L"Refresh", WS_CHILD | WS_VISIBLE,
                10, 410, 100, 28, hwnd, (HMENU)(INT_PTR)ID_LOG_REFRESH, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            SendMessageW(st->text, WM_SETFONT, (WPARAM)font, TRUE);
            SendMessageW(st->refresh, WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)st);
            return 0;
        }
        case WM_COMMAND: {
            State* st = GetState(hwnd);
            if (LOWORD(wParam) == ID_LOG_REFRESH && HIWORD(wParam) == BN_CLICKED && st->getLatest) {
                SetWindowTextW(st->text, st->getLatest().c_str());
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
