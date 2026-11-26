#include "BookmarkletRunnerDialog.h"

namespace {
    constexpr int ID_INPUT = 1901, ID_RUN = 1902, ID_RESULTS = 1903;
    const wchar_t* kClassName = L"BlackholeBookmarkletRunner";

    struct State {
        HWND input, results;
        BookmarkletRunnerDialog::ExecFn exec;
    };
    State* GetState(HWND h) { return reinterpret_cast<State*>(GetWindowLongPtr(h, GWLP_USERDATA)); }

    std::wstring GetText(HWND h) {
        int len = GetWindowTextLengthW(h);
        std::wstring s(len, L'\0');
        if (len > 0) GetWindowTextW(h, &s[0], len + 1);
        return s;
    }

    std::wstring StripJavascriptPrefix(std::wstring s) {
        size_t a = s.find_first_not_of(L" \t\r\n");
        if (a == std::wstring::npos) return s;
        s = s.substr(a);
        if (s.rfind(L"javascript:", 0) == 0) s = s.substr(11);
        return s;
    }
}

void BookmarkletRunnerDialog::RegisterClassOnce() {
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

void BookmarkletRunnerDialog::ShowFor(HWND owner, ExecFn exec) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - Bookmarklet Runner (active tab)",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 540, 420,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    State* st = GetState(hwnd);
    if (st) st->exec = std::move(exec);
    ShowWindow(hwnd, SW_SHOW);
}

void BookmarkletRunnerDialog::Run(HWND hwnd) {
    State* st = GetState(hwnd);
    std::wstring raw = GetText(st->input);
    std::wstring script = StripJavascriptPrefix(raw);
    if (script.empty() || !st->exec) return;
    SetWindowTextW(st->results, L"Running...");
    st->exec(script, [st](const std::wstring& result) {
        SetWindowTextW(st->results, result.c_str());
    });
}

LRESULT CALLBACK BookmarkletRunnerDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            State* st = new State();
            CreateWindowW(L"STATIC", L"Bookmarklet (javascript:... or bare JS):", WS_CHILD | WS_VISIBLE,
                10, 10, 400, 18, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            st->input = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | WS_VSCROLL,
                10, 30, 500, 100, hwnd, (HMENU)(INT_PTR)ID_INPUT, GetModuleHandleW(nullptr), nullptr);
            CreateWindowW(L"BUTTON", L"Run on active tab", WS_CHILD | WS_VISIBLE, 10, 138, 150, 28,
                hwnd, (HMENU)(INT_PTR)ID_RUN, GetModuleHandleW(nullptr), nullptr);
            st->results = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL, 10, 176, 500, 200,
                hwnd, (HMENU)(INT_PTR)ID_RESULTS, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HWND kids[] = { st->input, st->results };
            for (auto k : kids) SendMessageW(k, WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)st);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_RUN && HIWORD(wParam) == BN_CLICKED) Run(hwnd);
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
