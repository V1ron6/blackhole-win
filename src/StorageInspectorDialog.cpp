#include "StorageInspectorDialog.h"

namespace {
    constexpr int ID_REFRESH = 2801, ID_RESULTS = 2802;
    const wchar_t* kClassName = L"BlackholeStorageInspector";

    struct State {
        HWND refresh, results;
        StorageInspectorDialog::ExecFn exec;
    };
    State* GetState(HWND h) { return reinterpret_cast<State*>(GetWindowLongPtr(h, GWLP_USERDATA)); }

    // Returns a pre-formatted string (not raw JSON.stringify of the storage
    // objects) so the result needs no further parsing on the C++ side -
    // ExecuteScript already JSON-encodes whatever this returns, so the
    // dialog still needs to unwrap one layer of quoting.
    const wchar_t* kDumpScript =
        L"(function(){"
        L"function dump(store,label){"
        L"  var out=[label+' ('+store.length+' item(s)):'];"
        L"  for(var i=0;i<store.length;i++){ var k=store.key(i); out.push('  '+k+' = '+store.getItem(k)); }"
        L"  return out.join('\\n');"
        L"}"
        L"return dump(localStorage,'localStorage')+'\\n\\n'+dump(sessionStorage,'sessionStorage');"
        L"})()";

    std::wstring UnquoteJsonString(std::wstring s) {
        if (s.size() >= 2 && s.front() == L'"' && s.back() == L'"') s = s.substr(1, s.size() - 2);
        std::wstring out;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == L'\\' && i + 1 < s.size()) {
                wchar_t n = s[i + 1];
                if (n == L'n') { out += L"\r\n"; i++; continue; }
                if (n == L'"' || n == L'\\') { out += n; i++; continue; }
            }
            out += s[i];
        }
        return out;
    }
}

void StorageInspectorDialog::RegisterClassOnce() {
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

void StorageInspectorDialog::ShowFor(HWND owner, ExecFn exec) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - Storage Inspector (active tab, read-only)",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 520, 440,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    State* st = GetState(hwnd);
    if (st) {
        st->exec = std::move(exec);
        Refresh(hwnd);
    }
    ShowWindow(hwnd, SW_SHOW);
}

void StorageInspectorDialog::Refresh(HWND hwnd) {
    State* st = GetState(hwnd);
    if (!st->exec) return;
    SetWindowTextW(st->results, L"Loading...");
    st->exec(kDumpScript, [st](const std::wstring& result) {
        SetWindowTextW(st->results, UnquoteJsonString(result).c_str());
    });
}

LRESULT CALLBACK StorageInspectorDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            State* st = new State();
            st->refresh = CreateWindowW(L"BUTTON", L"Refresh", WS_CHILD | WS_VISIBLE,
                10, 10, 100, 26, hwnd, (HMENU)(INT_PTR)ID_REFRESH, GetModuleHandleW(nullptr), nullptr);
            st->results = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_HSCROLL, 10, 46, 480, 350,
                hwnd, (HMENU)(INT_PTR)ID_RESULTS, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            SendMessageW(st->refresh, WM_SETFONT, (WPARAM)font, TRUE);
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
