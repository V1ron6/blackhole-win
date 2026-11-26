#include "ViewportEmulatorDialog.h"
#include <string>

namespace {
    constexpr int ID_PRESETS = 3001, ID_WIDTH = 3002, ID_HEIGHT = 3003, ID_APPLY = 3004, ID_RESET = 3005;
    const wchar_t* kClassName = L"BlackholeViewportEmulator";

    struct Preset { const wchar_t* label; int w, h; };
    const Preset kPresets[] = {
        { L"iPhone SE (375 x 667)", 375, 667 },
        { L"iPhone 14 Pro (393 x 852)", 393, 852 },
        { L"Pixel 7 (412 x 915)", 412, 915 },
        { L"iPad (768 x 1024)", 768, 1024 },
        { L"Small laptop (1366 x 768)", 1366, 768 },
        { L"Desktop (1920 x 1080)", 1920, 1080 },
    };

    struct State {
        HWND presets, width, height;
        ViewportEmulatorDialog::SetFn setViewport;
        ViewportEmulatorDialog::ClearFn clearViewport;
    };
    State* GetState(HWND h) { return reinterpret_cast<State*>(GetWindowLongPtr(h, GWLP_USERDATA)); }

    std::wstring GetText(HWND h) {
        int len = GetWindowTextLengthW(h);
        std::wstring s(len, L'\0');
        if (len > 0) GetWindowTextW(h, &s[0], len + 1);
        return s;
    }
}

void ViewportEmulatorDialog::RegisterClassOnce() {
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

void ViewportEmulatorDialog::ShowFor(HWND owner, SetFn setViewport, ClearFn clearViewport) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - Viewport Emulator (active tab)",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 420, 260,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    State* st = GetState(hwnd);
    if (st) { st->setViewport = std::move(setViewport); st->clearViewport = std::move(clearViewport); }
    ShowWindow(hwnd, SW_SHOW);
}

LRESULT CALLBACK ViewportEmulatorDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            State* st = new State();
            CreateWindowW(L"STATIC", L"Preset:", WS_CHILD | WS_VISIBLE, 10, 10, 100, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            st->presets = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                10, 30, 380, 200, hwnd, (HMENU)(INT_PTR)ID_PRESETS, GetModuleHandleW(nullptr), nullptr);
            for (auto& p : kPresets) SendMessageW(st->presets, CB_ADDSTRING, 0, (LPARAM)p.label);
            SendMessageW(st->presets, CB_ADDSTRING, 0, (LPARAM)L"Custom...");

            CreateWindowW(L"STATIC", L"Width:", WS_CHILD | WS_VISIBLE, 10, 64, 60, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            st->width = CreateWindowW(L"EDIT", L"375", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                70, 62, 80, 22, hwnd, (HMENU)(INT_PTR)ID_WIDTH, GetModuleHandleW(nullptr), nullptr);
            CreateWindowW(L"STATIC", L"Height:", WS_CHILD | WS_VISIBLE, 160, 64, 60, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            st->height = CreateWindowW(L"EDIT", L"667", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                220, 62, 80, 22, hwnd, (HMENU)(INT_PTR)ID_HEIGHT, GetModuleHandleW(nullptr), nullptr);

            CreateWindowW(L"BUTTON", L"Apply", WS_CHILD | WS_VISIBLE, 10, 98, 100, 28,
                hwnd, (HMENU)(INT_PTR)ID_APPLY, GetModuleHandleW(nullptr), nullptr);
            CreateWindowW(L"BUTTON", L"Reset to full size", WS_CHILD | WS_VISIBLE, 120, 98, 140, 28,
                hwnd, (HMENU)(INT_PTR)ID_RESET, GetModuleHandleW(nullptr), nullptr);
            CreateWindowW(L"STATIC",
                L"Shrinks the browser area to this size (pinned top-left). "
                L"Does not draw a device frame or report a different devicePixelRatio to the page.",
                WS_CHILD | WS_VISIBLE, 10, 140, 380, 50, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HWND kids[] = { st->presets, st->width, st->height };
            for (auto k : kids) SendMessageW(k, WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)st);
            return 0;
        }
        case WM_COMMAND: {
            State* st = GetState(hwnd);
            if (LOWORD(wParam) == ID_PRESETS && HIWORD(wParam) == CBN_SELCHANGE) {
                int sel = (int)SendMessageW(st->presets, CB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < (int)(sizeof(kPresets) / sizeof(kPresets[0]))) {
                    SetWindowTextW(st->width, std::to_wstring(kPresets[sel].w).c_str());
                    SetWindowTextW(st->height, std::to_wstring(kPresets[sel].h).c_str());
                }
                return 0;
            }
            if (LOWORD(wParam) == ID_APPLY && HIWORD(wParam) == BN_CLICKED) {
                int w = _wtoi(GetText(st->width).c_str());
                int h = _wtoi(GetText(st->height).c_str());
                if (w > 0 && h > 0 && st->setViewport) st->setViewport(w, h);
                return 0;
            }
            if (LOWORD(wParam) == ID_RESET && HIWORD(wParam) == BN_CLICKED) {
                if (st->clearViewport) st->clearViewport();
                return 0;
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
