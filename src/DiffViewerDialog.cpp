#include "DiffViewerDialog.h"
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

namespace {
    constexpr int ID_TEXTA = 2201, ID_TEXTB = 2202, ID_DIFF = 2203, ID_RESULTS = 2204;
    const wchar_t* kClassName = L"BlackholeDiffViewer";

    struct Controls { HWND textA, textB, results; };
    Controls* GetControls(HWND h) { return reinterpret_cast<Controls*>(GetWindowLongPtr(h, GWLP_USERDATA)); }

    std::wstring GetText(HWND h) {
        int len = GetWindowTextLengthW(h);
        std::wstring s(len, L'\0');
        if (len > 0) GetWindowTextW(h, &s[0], len + 1);
        return s;
    }

    std::vector<std::wstring> SplitLines(const std::wstring& s) {
        std::vector<std::wstring> lines;
        std::wstringstream ss(s);
        std::wstring line;
        while (std::getline(ss, line)) {
            if (!line.empty() && line.back() == L'\r') line.pop_back();
            lines.push_back(line);
        }
        return lines;
    }

    // Standard LCS table, then backtrack to emit a unified-diff-style
    // (-removed / +added / unchanged) line list.
    std::wstring DiffLines(const std::vector<std::wstring>& a, const std::vector<std::wstring>& b) {
        size_t n = a.size(), m = b.size();
        std::vector<std::vector<int>> dp(n + 1, std::vector<int>(m + 1, 0));
        for (size_t i = n; i-- > 0;) {
            for (size_t j = m; j-- > 0;) {
                dp[i][j] = (a[i] == b[j]) ? dp[i + 1][j + 1] + 1 : std::max(dp[i + 1][j], dp[i][j + 1]);
            }
        }

        std::wstringstream out;
        size_t i = 0, j = 0;
        int same = 0, added = 0, removed = 0;
        while (i < n && j < m) {
            if (a[i] == b[j]) { out << L"  " << a[i] << L"\r\n"; i++; j++; same++; }
            else if (dp[i + 1][j] >= dp[i][j + 1]) { out << L"- " << a[i] << L"\r\n"; i++; removed++; }
            else { out << L"+ " << b[j] << L"\r\n"; j++; added++; }
        }
        while (i < n) { out << L"- " << a[i++] << L"\r\n"; removed++; }
        while (j < m) { out << L"+ " << b[j++] << L"\r\n"; added++; }

        std::wstringstream result;
        result << same << L" unchanged, " << added << L" added, " << removed << L" removed\r\n\r\n" << out.str();
        return result.str();
    }
}

void DiffViewerDialog::RegisterClassOnce() {
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

void DiffViewerDialog::ShowFor(HWND owner) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - Diff Viewer",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 620, 560,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(hwnd, SW_SHOW);
}

void DiffViewerDialog::RunDiff(HWND hwnd) {
    Controls* c = GetControls(hwnd);
    auto a = SplitLines(GetText(c->textA));
    auto b = SplitLines(GetText(c->textB));
    SetWindowTextW(c->results, DiffLines(a, b).c_str());
}

LRESULT CALLBACK DiffViewerDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            Controls* c = new Controls();
            CreateWindowW(L"STATIC", L"Text A:", WS_CHILD | WS_VISIBLE, 10, 10, 280, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            c->textA = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | WS_VSCROLL,
                10, 30, 280, 120, hwnd, (HMENU)(INT_PTR)ID_TEXTA, GetModuleHandleW(nullptr), nullptr);

            CreateWindowW(L"STATIC", L"Text B:", WS_CHILD | WS_VISIBLE, 300, 10, 280, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            c->textB = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | WS_VSCROLL,
                300, 30, 280, 120, hwnd, (HMENU)(INT_PTR)ID_TEXTB, GetModuleHandleW(nullptr), nullptr);

            CreateWindowW(L"BUTTON", L"Diff", WS_CHILD | WS_VISIBLE, 10, 158, 100, 28,
                hwnd, (HMENU)(INT_PTR)ID_DIFF, GetModuleHandleW(nullptr), nullptr);

            c->results = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_HSCROLL, 10, 196, 570, 320,
                hwnd, (HMENU)(INT_PTR)ID_RESULTS, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HWND kids[] = { c->textA, c->textB, c->results };
            for (auto k : kids) SendMessageW(k, WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)c);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_DIFF && HIWORD(wParam) == BN_CLICKED) RunDiff(hwnd);
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
