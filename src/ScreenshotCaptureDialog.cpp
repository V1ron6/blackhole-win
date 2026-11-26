#include "ScreenshotCaptureDialog.h"
#include <commdlg.h>
#include <shlwapi.h>

#pragma comment(lib, "comdlg32.lib")

namespace {
    constexpr int ID_PATH = 2701, ID_BROWSE = 2702, ID_CAPTURE = 2703, ID_STATUS = 2704;
    const wchar_t* kClassName = L"BlackholeScreenshotCapture";

    struct State {
        HWND path, status;
        ScreenshotCaptureDialog::CaptureFn capture;
    };
    State* GetState(HWND h) { return reinterpret_cast<State*>(GetWindowLongPtr(h, GWLP_USERDATA)); }

    std::wstring GetText(HWND h) {
        int len = GetWindowTextLengthW(h);
        std::wstring s(len, L'\0');
        if (len > 0) GetWindowTextW(h, &s[0], len + 1);
        return s;
    }
}

void ScreenshotCaptureDialog::RegisterClassOnce() {
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

void ScreenshotCaptureDialog::ShowFor(HWND owner, CaptureFn capture) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - Screenshot Capture (active tab)",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 520, 220,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    State* st = GetState(hwnd);
    if (st) st->capture = std::move(capture);
    ShowWindow(hwnd, SW_SHOW);
}

void ScreenshotCaptureDialog::RunCapture(HWND hwnd) {
    State* st = GetState(hwnd);
    std::wstring path = GetText(st->path);
    if (path.empty()) {
        wchar_t temp[MAX_PATH];
        GetTempPathW(MAX_PATH, temp);
        path = std::wstring(temp) + L"blackhole-screenshot.png";
        SetWindowTextW(st->path, path.c_str());
    }
    if (!st->capture) return;
    SetWindowTextW(st->status, L"Capturing...");
    st->capture(path, [st](bool ok, const std::wstring& pathOrError) {
        SetWindowTextW(st->status, ok ? (L"Saved to " + pathOrError).c_str() : (L"Failed: " + pathOrError).c_str());
    });
}

LRESULT CALLBACK ScreenshotCaptureDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            State* st = new State();
            CreateWindowW(L"STATIC", L"Save as (leave blank for a temp file):", WS_CHILD | WS_VISIBLE,
                10, 10, 300, 18, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            st->path = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
                10, 30, 390, 24, hwnd, (HMENU)(INT_PTR)ID_PATH, GetModuleHandleW(nullptr), nullptr);
            CreateWindowW(L"BUTTON", L"Browse...", WS_CHILD | WS_VISIBLE, 410, 30, 90, 24,
                hwnd, (HMENU)(INT_PTR)ID_BROWSE, GetModuleHandleW(nullptr), nullptr);
            CreateWindowW(L"BUTTON", L"Capture active tab", WS_CHILD | WS_VISIBLE, 10, 64, 160, 28,
                hwnd, (HMENU)(INT_PTR)ID_CAPTURE, GetModuleHandleW(nullptr), nullptr);
            st->status = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 10, 102, 480, 60,
                hwnd, (HMENU)(INT_PTR)ID_STATUS, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HWND kids[] = { st->path, st->status };
            for (auto k : kids) SendMessageW(k, WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)st);
            return 0;
        }
        case WM_COMMAND: {
            State* st = GetState(hwnd);
            if (LOWORD(wParam) == ID_BROWSE && HIWORD(wParam) == BN_CLICKED) {
                wchar_t fileBuf[MAX_PATH] = L"blackhole-screenshot.png";
                OPENFILENAMEW ofn{};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hwnd;
                ofn.lpstrFile = fileBuf;
                ofn.nMaxFile = MAX_PATH;
                ofn.lpstrFilter = L"PNG image (*.png)\0*.png\0";
                ofn.lpstrDefExt = L"png";
                ofn.Flags = OFN_OVERWRITEPROMPT;
                if (GetSaveFileNameW(&ofn)) SetWindowTextW(st->path, fileBuf);
                return 0;
            }
            if (LOWORD(wParam) == ID_CAPTURE && HIWORD(wParam) == BN_CLICKED) {
                RunCapture(hwnd);
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
