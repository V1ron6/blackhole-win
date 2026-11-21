#include "RegexTesterDialog.h"
#include <regex>
#include <string>
#include <sstream>
#include <cstring>

namespace {
    constexpr int ID_PATTERN = 701, ID_TEXT = 702, ID_CASEINSENSITIVE = 703, ID_TEST = 704, ID_RESULTS = 705;
    const wchar_t* kClassName = L"BlackholeRegexTester";

    struct Controls { HWND pattern, text, caseInsensitive, results; };
    Controls* GetControls(HWND h) { return reinterpret_cast<Controls*>(GetWindowLongPtr(h, GWLP_USERDATA)); }

    std::wstring GetText(HWND h) {
        int len = GetWindowTextLengthW(h);
        std::wstring s(len, L'\0');
        if (len > 0) GetWindowTextW(h, &s[0], len + 1);
        return s;
    }

    std::wstring FromAscii(const char* s) {
        return std::wstring(s, s + strlen(s));
    }
}

void RegexTesterDialog::RegisterClassOnce() {
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

void RegexTesterDialog::ShowFor(HWND owner) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - Regex Tester",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 560, 480,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(hwnd, SW_SHOW);
}

void RegexTesterDialog::RunTest(HWND hwnd) {
    Controls* c = GetControls(hwnd);
    std::wstring pattern = GetText(c->pattern);
    std::wstring text    = GetText(c->text);
    bool ci = SendMessageW(c->caseInsensitive, BM_GETCHECK, 0, 0) == BST_CHECKED;

    std::wstringstream out;
    try {
        auto flags = std::regex::ECMAScript;
        if (ci) flags |= std::regex::icase;
        std::wregex re(pattern, flags);

        auto begin = std::wsregex_iterator(text.begin(), text.end(), re);
        auto end = std::wsregex_iterator();
        int count = 0;
        for (auto it = begin; it != end; ++it, ++count) {
            const std::wsmatch& m = *it;
            out << L"Match " << (count + 1) << L" at " << m.position(0) << L": \"" << m.str(0) << L"\"\r\n";
            for (size_t g = 1; g < m.size(); ++g) {
                out << L"    group " << g << L": \"" << (m[g].matched ? m.str(g) : L"(no match)") << L"\"\r\n";
            }
        }
        if (count == 0) out << L"No matches.";
        else out << L"\r\n" << count << L" match(es) total.";
    } catch (const std::regex_error& e) {
        out << L"Invalid regex: " << FromAscii(e.what());
    }
    SetWindowTextW(c->results, out.str().c_str());
}

LRESULT CALLBACK RegexTesterDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            Controls* c = new Controls();
            CreateWindowW(L"STATIC", L"Pattern (ECMAScript regex):", WS_CHILD | WS_VISIBLE, 10, 10, 300, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            c->pattern = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
                10, 30, 440, 24, hwnd, (HMENU)(INT_PTR)ID_PATTERN, GetModuleHandleW(nullptr), nullptr);
            c->caseInsensitive = CreateWindowW(L"BUTTON", L"Ignore case", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                10, 60, 120, 22, hwnd, (HMENU)(INT_PTR)ID_CASEINSENSITIVE, GetModuleHandleW(nullptr), nullptr);

            CreateWindowW(L"STATIC", L"Test text:", WS_CHILD | WS_VISIBLE, 10, 90, 200, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            c->text = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | WS_VSCROLL,
                10, 110, 520, 130, hwnd, (HMENU)(INT_PTR)ID_TEXT, GetModuleHandleW(nullptr), nullptr);

            CreateWindowW(L"BUTTON", L"Test", WS_CHILD | WS_VISIBLE, 10, 248, 100, 26,
                hwnd, (HMENU)(INT_PTR)ID_TEST, GetModuleHandleW(nullptr), nullptr);

            CreateWindowW(L"STATIC", L"Matches:", WS_CHILD | WS_VISIBLE, 10, 282, 200, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            c->results = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL, 10, 302, 520, 130,
                hwnd, (HMENU)(INT_PTR)ID_RESULTS, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HWND kids[] = { c->pattern, c->caseInsensitive, c->text, c->results };
            for (auto k : kids) SendMessageW(k, WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)c);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_TEST && HIWORD(wParam) == BN_CLICKED) RunTest(hwnd);
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
