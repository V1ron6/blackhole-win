#include "JsConsoleDialog.h"

namespace {
    constexpr int ID_INPUT  = 201;
    constexpr int ID_RUN    = 202;
    constexpr int ID_OUTPUT = 203;
    const wchar_t* kClassName = L"BlackholeJsConsole";

    struct State {
        HWND input, run, output;
        JsConsoleDialog::ExecFn exec;
        std::wstring history; // simple scrollback, newest at bottom - mirrors
                               // JavaScriptConsole.kt's "console history" field
    };

    State* GetState(HWND h) { return reinterpret_cast<State*>(GetWindowLongPtr(h, GWLP_USERDATA)); }

    std::wstring GetText(HWND h) {
        int len = GetWindowTextLengthW(h);
        std::wstring s(len, L'\0');
        if (len > 0) GetWindowTextW(h, &s[0], len + 1);
        return s;
    }
}

void JsConsoleDialog::RegisterClassOnce() {
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

void JsConsoleDialog::ShowFor(HWND owner, ExecFn exec) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - JavaScript Console (Advance mode)",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 560, 420,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    State* st = GetState(hwnd);
    if (st) st->exec = std::move(exec);
    ShowWindow(hwnd, SW_SHOW);
}

LRESULT CALLBACK JsConsoleDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            State* st = new State();
            st->output = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL,
                10, 10, 520, 280, hwnd, (HMENU)(INT_PTR)ID_OUTPUT, GetModuleHandleW(nullptr), nullptr);
            st->input = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
                10, 300, 420, 26, hwnd, (HMENU)(INT_PTR)ID_INPUT, GetModuleHandleW(nullptr), nullptr);
            st->run = CreateWindowW(L"BUTTON", L"Run", WS_CHILD | WS_VISIBLE,
                440, 300, 90, 26, hwnd, (HMENU)(INT_PTR)ID_RUN, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            SendMessageW(st->output, WM_SETFONT, (WPARAM)font, TRUE);
            SendMessageW(st->input,  WM_SETFONT, (WPARAM)font, TRUE);
            SendMessageW(st->run,    WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)st);
            return 0;
        }
        case WM_COMMAND: {
            State* st = GetState(hwnd);
            bool enterInInput = (LOWORD(wParam) == ID_INPUT && HIWORD(wParam) == EN_CHANGE && false);
            if ((LOWORD(wParam) == ID_RUN && HIWORD(wParam) == BN_CLICKED) || enterInInput) {
                std::wstring script = GetText(st->input);
                if (script.empty() || !st->exec) return 0;
                st->history += L"> " + script + L"\r\n";
                SetWindowTextW(st->output, st->history.c_str());
                auto* stCopy = st;
                stCopy->exec(script, [stCopy, hwnd](const std::wstring& resultJson) {
                    stCopy->history += resultJson + L"\r\n";
                    SetWindowTextW(stCopy->output, stCopy->history.c_str());
                    int len = GetWindowTextLengthW(stCopy->output);
                    SendMessageW(stCopy->output, EM_SETSEL, len, len);
                    SendMessageW(stCopy->output, EM_SCROLLCARET, 0, 0);
                });
                SetWindowTextW(st->input, L"");
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
