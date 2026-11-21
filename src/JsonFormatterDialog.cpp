#include "JsonFormatterDialog.h"
#include <string>
#include <cwctype>

namespace {
    constexpr int ID_INPUT = 501, ID_OUTPUT = 502, ID_PRETTY = 503, ID_MINIFY = 504;
    const wchar_t* kClassName = L"BlackholeJsonFormatter";

    struct Controls { HWND input, output; };
    Controls* GetControls(HWND h) { return reinterpret_cast<Controls*>(GetWindowLongPtr(h, GWLP_USERDATA)); }

    std::wstring GetText(HWND h) {
        int len = GetWindowTextLengthW(h);
        std::wstring s(len, L'\0');
        if (len > 0) GetWindowTextW(h, &s[0], len + 1);
        return s;
    }

    // Structural pretty-printer: walks the text once, tracking whether
    // we're inside a string literal (and whether the next char is escaped)
    // so braces/brackets/commas inside strings are left alone.
    std::wstring PrettyPrint(const std::wstring& in) {
        std::wstring out;
        int depth = 0;
        bool inString = false, escaped = false;

        auto newlineIndent = [&](int d) {
            out += L'\n';
            for (int i = 0; i < d; ++i) out += L"  ";
        };

        for (size_t i = 0; i < in.size(); ++i) {
            wchar_t c = in[i];
            if (inString) {
                out += c;
                if (escaped) escaped = false;
                else if (c == L'\\') escaped = true;
                else if (c == L'"') inString = false;
                continue;
            }
            switch (c) {
                case L'"': inString = true; out += c; break;
                case L'{': case L'[':
                    out += c; depth++; newlineIndent(depth);
                    break;
                case L'}': case L']':
                    depth = depth > 0 ? depth - 1 : 0;
                    newlineIndent(depth); out += c;
                    break;
                case L',':
                    out += c; newlineIndent(depth);
                    break;
                case L':':
                    out += L": ";
                    break;
                default:
                    if (!iswspace(c)) out += c; // drop original whitespace/newlines
                    break;
            }
        }
        return out;
    }

    std::wstring Minify(const std::wstring& in) {
        std::wstring out;
        bool inString = false, escaped = false;
        for (wchar_t c : in) {
            if (inString) {
                out += c;
                if (escaped) escaped = false;
                else if (c == L'\\') escaped = true;
                else if (c == L'"') inString = false;
                continue;
            }
            if (c == L'"') { inString = true; out += c; continue; }
            if (!iswspace(c)) out += c;
        }
        return out;
    }
}

void JsonFormatterDialog::RegisterClassOnce() {
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

void JsonFormatterDialog::ShowFor(HWND owner) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - JSON Formatter",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 560, 520,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(hwnd, SW_SHOW);
}

void JsonFormatterDialog::Run(HWND hwnd, bool pretty) {
    Controls* c = GetControls(hwnd);
    std::wstring in = GetText(c->input);
    SetWindowTextW(c->output, (pretty ? PrettyPrint(in) : Minify(in)).c_str());
}

LRESULT CALLBACK JsonFormatterDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            Controls* c = new Controls();
            CreateWindowW(L"STATIC", L"Input JSON:", WS_CHILD | WS_VISIBLE, 10, 10, 200, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            c->input = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | WS_VSCROLL,
                10, 30, 520, 180, hwnd, (HMENU)(INT_PTR)ID_INPUT, GetModuleHandleW(nullptr), nullptr);

            CreateWindowW(L"BUTTON", L"Pretty-print", WS_CHILD | WS_VISIBLE, 10, 216, 110, 26,
                hwnd, (HMENU)(INT_PTR)ID_PRETTY, GetModuleHandleW(nullptr), nullptr);
            CreateWindowW(L"BUTTON", L"Minify", WS_CHILD | WS_VISIBLE, 130, 216, 90, 26,
                hwnd, (HMENU)(INT_PTR)ID_MINIFY, GetModuleHandleW(nullptr), nullptr);

            CreateWindowW(L"STATIC", L"Output:", WS_CHILD | WS_VISIBLE, 10, 252, 200, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            c->output = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_HSCROLL, 10, 272, 520, 210,
                hwnd, (HMENU)(INT_PTR)ID_OUTPUT, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            SendMessageW(c->input, WM_SETFONT, (WPARAM)font, TRUE);
            SendMessageW(c->output, WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)c);
            return 0;
        }
        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED) {
                if (LOWORD(wParam) == ID_PRETTY) Run(hwnd, true);
                else if (LOWORD(wParam) == ID_MINIFY) Run(hwnd, false);
            }
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
