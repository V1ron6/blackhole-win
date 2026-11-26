#include "AccessibilityCheckerDialog.h"

namespace {
    constexpr int ID_CHECK = 1801, ID_RESULTS = 1802;
    const wchar_t* kClassName = L"BlackholeAccessibilityChecker";

    struct State {
        HWND check, results;
        AccessibilityCheckerDialog::ExecFn exec;
    };
    State* GetState(HWND h) { return reinterpret_cast<State*>(GetWindowLongPtr(h, GWLP_USERDATA)); }

    // ExecuteScript's result is the JS value JSON-encoded, so a JS string
    // comes back as a quoted, backslash-escaped JSON string - unwrap that
    // for display instead of showing the user literal \n and \" escapes.
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

    // Same checks AccessibilityChecker.kt ran: missing alt text, missing
    // form labels, missing <html lang>, missing <title>. Returns a
    // newline-joined report string.
    const wchar_t* kCheckScript =
        L"(function(){"
        L"var lines=[];"
        L"var imgs=document.querySelectorAll('img');"
        L"var badImgs=0; imgs.forEach(function(i){ if(!i.hasAttribute('alt')) badImgs++; });"
        L"lines.push('Images missing alt text: '+badImgs+' / '+imgs.length);"
        L"var inputs=document.querySelectorAll('input,select,textarea');"
        L"var badInputs=0; inputs.forEach(function(el){"
        L"  var id=el.getAttribute('id');"
        L"  var hasLabel = id && document.querySelector('label[for=\"'+id+'\"]');"
        L"  var hasAria = el.hasAttribute('aria-label') || el.hasAttribute('aria-labelledby');"
        L"  if(!hasLabel && !hasAria) badInputs++;"
        L"});"
        L"lines.push('Form fields missing a label: '+badInputs+' / '+inputs.length);"
        L"lines.push('<html lang> attribute: '+(document.documentElement.getAttribute('lang')||'(missing)'));"
        L"lines.push('<title>: '+(document.title || '(missing)'));"
        L"return lines.join('\\n');"
        L"})()";
}

void AccessibilityCheckerDialog::RegisterClassOnce() {
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

void AccessibilityCheckerDialog::ShowFor(HWND owner, ExecFn exec) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - Accessibility Checker (active tab)",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 480, 360,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    State* st = GetState(hwnd);
    if (st) st->exec = std::move(exec);
    ShowWindow(hwnd, SW_SHOW);
}

LRESULT CALLBACK AccessibilityCheckerDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            State* st = new State();
            st->check = CreateWindowW(L"BUTTON", L"Check active tab", WS_CHILD | WS_VISIBLE,
                10, 10, 160, 28, hwnd, (HMENU)(INT_PTR)ID_CHECK, GetModuleHandleW(nullptr), nullptr);
            st->results = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL, 10, 48, 440, 270,
                hwnd, (HMENU)(INT_PTR)ID_RESULTS, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            SendMessageW(st->check, WM_SETFONT, (WPARAM)font, TRUE);
            SendMessageW(st->results, WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)st);
            return 0;
        }
        case WM_COMMAND: {
            State* st = GetState(hwnd);
            if (LOWORD(wParam) == ID_CHECK && HIWORD(wParam) == BN_CLICKED && st->exec) {
                SetWindowTextW(st->results, L"Checking...");
                st->exec(kCheckScript, [st](const std::wstring& result) {
                    SetWindowTextW(st->results, UnquoteJsonString(result).c_str());
                });
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
