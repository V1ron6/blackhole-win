#include "RequestTimelineDialog.h"
#include <sstream>
#include <vector>

namespace {
    constexpr int ID_REFRESH = 2601, ID_RESULTS = 2602;
    const wchar_t* kClassName = L"BlackholeRequestTimeline";

    struct State {
        HWND results;
        RequestTimelineDialog::GetLogFn getLog;
    };
    State* GetState(HWND h) { return reinterpret_cast<State*>(GetWindowLongPtr(h, GWLP_USERDATA)); }

    // Tab::requestLog is newest-first (each new entry is prepended) - a
    // timeline reads better oldest-first, so just reverse the line order.
    std::wstring ChronologicalOrder(const std::wstring& log) {
        std::vector<std::wstring> lines;
        std::wstringstream ss(log);
        std::wstring line;
        while (std::getline(ss, line)) {
            if (!line.empty() && line.back() == L'\r') line.pop_back();
            if (!line.empty()) lines.push_back(line);
        }
        std::wstringstream out;
        for (auto it = lines.rbegin(); it != lines.rend(); ++it) out << *it << L"\r\n";
        if (lines.empty()) out << L"(no requests logged yet for this tab)";
        return out.str();
    }

    void Refresh(HWND hwnd) {
        State* st = GetState(hwnd);
        if (st->getLog) SetWindowTextW(st->results, ChronologicalOrder(st->getLog()).c_str());
    }
}

void RequestTimelineDialog::RegisterClassOnce() {
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

void RequestTimelineDialog::ShowFor(HWND owner, GetLogFn getLog) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - Request Timeline (active tab, chronological)",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 620, 480,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    State* st = GetState(hwnd);
    if (st) {
        st->getLog = std::move(getLog);
        Refresh(hwnd);
    }
    ShowWindow(hwnd, SW_SHOW);
}

LRESULT CALLBACK RequestTimelineDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            State* st = new State();
            CreateWindowW(L"BUTTON", L"Refresh", WS_CHILD | WS_VISIBLE, 10, 10, 100, 26,
                hwnd, (HMENU)(INT_PTR)ID_REFRESH, GetModuleHandleW(nullptr), nullptr);
            st->results = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_HSCROLL, 10, 46, 580, 390,
                hwnd, (HMENU)(INT_PTR)ID_RESULTS, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            SendMessageW(st->results, WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)st);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_REFRESH && HIWORD(wParam) == BN_CLICKED) Refresh(hwnd);
            return 0;
        case WM_DESTROY:
            delete GetState(hwnd);
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
