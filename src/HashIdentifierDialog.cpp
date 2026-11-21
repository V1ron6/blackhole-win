#include "HashIdentifierDialog.h"
#include <string>
#include <sstream>
#include <vector>
#include <cwctype>

namespace {
    constexpr int ID_INPUT = 801, ID_IDENTIFY = 802, ID_RESULTS = 803;
    const wchar_t* kClassName = L"BlackholeHashIdentifier";

    struct Controls { HWND input, results; };
    Controls* GetControls(HWND h) { return reinterpret_cast<Controls*>(GetWindowLongPtr(h, GWLP_USERDATA)); }

    std::wstring GetText(HWND h) {
        int len = GetWindowTextLengthW(h);
        std::wstring s(len, L'\0');
        if (len > 0) GetWindowTextW(h, &s[0], len + 1);
        return s;
    }

    bool IsHex(const std::wstring& s) {
        for (wchar_t c : s) if (!iswxdigit(c)) return false;
        return !s.empty();
    }

    bool IsBase64Charset(const std::wstring& s) {
        for (wchar_t c : s) {
            if (!(iswalnum(c) || c == L'+' || c == L'/' || c == L'=')) return false;
        }
        return !s.empty();
    }

    // Same candidate table HashIdentifier.kt used: length + charset only,
    // explicitly not a crypto proof (several algorithms share a length).
    std::vector<std::wstring> Candidates(const std::wstring& h) {
        std::vector<std::wstring> out;
        size_t len = h.size();

        if (h.rfind(L"$2a$", 0) == 0 || h.rfind(L"$2b$", 0) == 0 || h.rfind(L"$2y$", 0) == 0) {
            out.push_back(L"bcrypt");
        }
        if (h.rfind(L"$1$", 0) == 0) out.push_back(L"MD5 crypt (Unix)");
        if (h.rfind(L"$5$", 0) == 0) out.push_back(L"SHA-256 crypt (Unix)");
        if (h.rfind(L"$6$", 0) == 0) out.push_back(L"SHA-512 crypt (Unix)");
        if (h.rfind(L"$argon2", 0) == 0) out.push_back(L"Argon2");

        if (IsHex(h)) {
            switch (len) {
                case 32: out.push_back(L"MD5"); out.push_back(L"NTLM"); out.push_back(L"MD4"); break;
                case 40: out.push_back(L"SHA-1"); out.push_back(L"RIPEMD-160"); break;
                case 56: out.push_back(L"SHA-224"); break;
                case 64: out.push_back(L"SHA-256"); out.push_back(L"SHA3-256"); break;
                case 96: out.push_back(L"SHA-384"); break;
                case 128: out.push_back(L"SHA-512"); out.push_back(L"SHA3-512"); break;
                default: break;
            }
        } else if (IsBase64Charset(h) && len >= 20 && len <= 44) {
            out.push_back(L"Possibly a Base64-encoded hash digest");
        }

        if (out.empty()) out.push_back(L"Unrecognized format - not a common hash length/charset");
        return out;
    }
}

void HashIdentifierDialog::RegisterClassOnce() {
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

void HashIdentifierDialog::ShowFor(HWND owner) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - Hash Identifier",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 480, 340,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(hwnd, SW_SHOW);
}

void HashIdentifierDialog::Identify(HWND hwnd) {
    Controls* c = GetControls(hwnd);
    std::wstring h = GetText(c->input);
    // Trim whitespace the user may have pasted along with the hash.
    size_t a = h.find_first_not_of(L" \t\r\n");
    size_t b = h.find_last_not_of(L" \t\r\n");
    h = (a == std::wstring::npos) ? L"" : h.substr(a, b - a + 1);

    std::wstringstream out;
    out << L"Length: " << h.size() << L" characters\r\n\r\nPossible formats:\r\n";
    for (auto& cand : Candidates(h)) out << L"  - " << cand << L"\r\n";
    out << L"\r\n(Pattern-matched by length/charset only - not a cryptographic proof.)";
    SetWindowTextW(c->results, out.str().c_str());
}

LRESULT CALLBACK HashIdentifierDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            Controls* c = new Controls();
            CreateWindowW(L"STATIC", L"Hash:", WS_CHILD | WS_VISIBLE, 10, 10, 100, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            c->input = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
                10, 30, 440, 24, hwnd, (HMENU)(INT_PTR)ID_INPUT, GetModuleHandleW(nullptr), nullptr);
            CreateWindowW(L"BUTTON", L"Identify", WS_CHILD | WS_VISIBLE, 10, 62, 100, 26,
                hwnd, (HMENU)(INT_PTR)ID_IDENTIFY, GetModuleHandleW(nullptr), nullptr);
            c->results = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL, 10, 98, 440, 190,
                hwnd, (HMENU)(INT_PTR)ID_RESULTS, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HWND kids[] = { c->input, c->results };
            for (auto k : kids) SendMessageW(k, WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)c);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_IDENTIFY && HIWORD(wParam) == BN_CLICKED) Identify(hwnd);
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
