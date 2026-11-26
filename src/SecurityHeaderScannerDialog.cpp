#include "SecurityHeaderScannerDialog.h"
#include "HttpFetch.h"
#include <string>
#include <sstream>
#include <vector>

namespace {
    constexpr int ID_URL = 1401, ID_SCAN = 1402, ID_RESULTS = 1403;
    const wchar_t* kClassName = L"BlackholeSecurityHeaderScanner";

    struct Controls { HWND url, results; };
    Controls* GetControls(HWND h) { return reinterpret_cast<Controls*>(GetWindowLongPtr(h, GWLP_USERDATA)); }

    std::wstring GetText(HWND h) {
        int len = GetWindowTextLengthW(h);
        std::wstring s(len, L'\0');
        if (len > 0) GetWindowTextW(h, &s[0], len + 1);
        return s;
    }

    // Same checklist SecurityHeaderScanner.kt used.
    const wchar_t* kChecklist[] = {
        L"Strict-Transport-Security", L"Content-Security-Policy", L"X-Content-Type-Options",
        L"X-Frame-Options", L"Referrer-Policy", L"Permissions-Policy", L"X-XSS-Protection",
    };
}

void SecurityHeaderScannerDialog::RegisterClassOnce() {
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

void SecurityHeaderScannerDialog::ShowFor(HWND owner) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - Security Header Scanner",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 560, 420,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(hwnd, SW_SHOW);
}

void SecurityHeaderScannerDialog::RunScan(HWND hwnd) {
    Controls* c = GetControls(hwnd);
    std::wstring url = GetText(c->url);
    SetWindowTextW(c->results, L"Scanning...");

    HttpFetchResult r = HttpFetchGet(url, /*headOnly=*/true);
    std::wstringstream out;
    if (!r.ok) {
        out << L"Request failed: " << r.error;
    } else {
        out << L"HTTP " << r.statusCode << L"\r\n\r\n";
        for (auto name : kChecklist) {
            std::wstring value = HttpFindHeader(r.rawHeaders, name);
            out << (value.empty() ? L"[MISSING] " : L"[present] ") << name;
            if (!value.empty()) out << L"  =  " << value;
            out << L"\r\n";
        }
    }
    SetWindowTextW(c->results, out.str().c_str());
}

LRESULT CALLBACK SecurityHeaderScannerDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            Controls* c = new Controls();
            CreateWindowW(L"STATIC", L"URL:", WS_CHILD | WS_VISIBLE, 10, 10, 80, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            c->url = CreateWindowW(L"EDIT", L"https://", WS_CHILD | WS_VISIBLE | WS_BORDER,
                10, 30, 400, 24, hwnd, (HMENU)(INT_PTR)ID_URL, GetModuleHandleW(nullptr), nullptr);
            CreateWindowW(L"BUTTON", L"Scan", WS_CHILD | WS_VISIBLE, 420, 30, 110, 24,
                hwnd, (HMENU)(INT_PTR)ID_SCAN, GetModuleHandleW(nullptr), nullptr);
            c->results = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL, 10, 64, 520, 310,
                hwnd, (HMENU)(INT_PTR)ID_RESULTS, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HWND kids[] = { c->url, c->results };
            for (auto k : kids) SendMessageW(k, WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)c);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_SCAN && HIWORD(wParam) == BN_CLICKED) RunScan(hwnd);
            return 0;
        case WM_DESTROY:
            delete GetControls(hwnd);
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
