#include "CipherSolverDialog.h"
#include <string>
#include <sstream>
#include <cwctype>

namespace {
    constexpr int ID_INPUT = 2001, ID_ROT13 = 2002, ID_ATBASH = 2003, ID_CAESAR_ALL = 2004, ID_RESULTS = 2005;
    const wchar_t* kClassName = L"BlackholeCipherSolver";

    struct Controls { HWND input, results; };
    Controls* GetControls(HWND h) { return reinterpret_cast<Controls*>(GetWindowLongPtr(h, GWLP_USERDATA)); }

    std::wstring GetText(HWND h) {
        int len = GetWindowTextLengthW(h);
        std::wstring s(len, L'\0');
        if (len > 0) GetWindowTextW(h, &s[0], len + 1);
        return s;
    }

    std::wstring CaesarShift(const std::wstring& in, int shift) {
        std::wstring out;
        out.reserve(in.size());
        for (wchar_t c : in) {
            if (c >= L'a' && c <= L'z') out += (wchar_t)(L'a' + ((c - L'a' + shift) % 26 + 26) % 26);
            else if (c >= L'A' && c <= L'Z') out += (wchar_t)(L'A' + ((c - L'A' + shift) % 26 + 26) % 26);
            else out += c;
        }
        return out;
    }

    std::wstring Atbash(const std::wstring& in) {
        std::wstring out;
        out.reserve(in.size());
        for (wchar_t c : in) {
            if (c >= L'a' && c <= L'z') out += (wchar_t)(L'z' - (c - L'a'));
            else if (c >= L'A' && c <= L'Z') out += (wchar_t)(L'Z' - (c - L'A'));
            else out += c;
        }
        return out;
    }
}

void CipherSolverDialog::RegisterClassOnce() {
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

void CipherSolverDialog::ShowFor(HWND owner) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - Cipher Solver",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 520, 480,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(hwnd, SW_SHOW);
}

void CipherSolverDialog::Run(HWND hwnd, int action) {
    Controls* c = GetControls(hwnd);
    std::wstring in = GetText(c->input);
    std::wstringstream out;
    switch (action) {
        case ID_ROT13:
            out << CaesarShift(in, 13);
            break;
        case ID_ATBASH:
            out << Atbash(in);
            break;
        case ID_CAESAR_ALL:
            for (int shift = 1; shift <= 25; ++shift)
                out << L"Shift " << shift << L": " << CaesarShift(in, shift) << L"\r\n";
            break;
    }
    SetWindowTextW(c->results, out.str().c_str());
}

LRESULT CALLBACK CipherSolverDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            Controls* c = new Controls();
            CreateWindowW(L"STATIC", L"Text:", WS_CHILD | WS_VISIBLE, 10, 10, 100, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            c->input = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | WS_VSCROLL,
                10, 30, 480, 70, hwnd, (HMENU)(INT_PTR)ID_INPUT, GetModuleHandleW(nullptr), nullptr);

            CreateWindowW(L"BUTTON", L"ROT13", WS_CHILD | WS_VISIBLE, 10, 108, 100, 26,
                hwnd, (HMENU)(INT_PTR)ID_ROT13, GetModuleHandleW(nullptr), nullptr);
            CreateWindowW(L"BUTTON", L"Atbash", WS_CHILD | WS_VISIBLE, 120, 108, 100, 26,
                hwnd, (HMENU)(INT_PTR)ID_ATBASH, GetModuleHandleW(nullptr), nullptr);
            CreateWindowW(L"BUTTON", L"Try all Caesar shifts", WS_CHILD | WS_VISIBLE, 230, 108, 160, 26,
                hwnd, (HMENU)(INT_PTR)ID_CAESAR_ALL, GetModuleHandleW(nullptr), nullptr);

            c->results = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL, 10, 144, 480, 300,
                hwnd, (HMENU)(INT_PTR)ID_RESULTS, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HWND kids[] = { c->input, c->results };
            for (auto k : kids) SendMessageW(k, WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)c);
            return 0;
        }
        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED) Run(hwnd, LOWORD(wParam));
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
