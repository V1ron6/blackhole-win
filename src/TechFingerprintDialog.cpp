#include "TechFingerprintDialog.h"
#include "HttpFetch.h"
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cwctype>

namespace {
    constexpr int ID_URL = 1601, ID_SCAN = 1602, ID_RESULTS = 1603;
    const wchar_t* kClassName = L"BlackholeTechFingerprint";

    struct Controls { HWND url, results; };
    Controls* GetControls(HWND h) { return reinterpret_cast<Controls*>(GetWindowLongPtr(h, GWLP_USERDATA)); }

    std::wstring GetText(HWND h) {
        int len = GetWindowTextLengthW(h);
        std::wstring s(len, L'\0');
        if (len > 0) GetWindowTextW(h, &s[0], len + 1);
        return s;
    }

    std::wstring FromUtf8(const std::vector<unsigned char>& bytes) {
        if (bytes.empty()) return {};
        int len = MultiByteToWideChar(CP_UTF8, 0, (const char*)bytes.data(), (int)bytes.size(), nullptr, 0);
        std::wstring w(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, (const char*)bytes.data(), (int)bytes.size(), &w[0], len);
        return w;
    }

    std::wstring Lower(std::wstring s) {
        std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return (wchar_t)towlower(c); });
        return s;
    }

    // Same marker table TechFingerprint.kt used: a body/header substring
    // that's a reasonably strong signal for the named technology. This is
    // pattern-matching on public markup, same technique Wappalyzer uses.
    struct Marker { const wchar_t* needle; const wchar_t* tech; bool inHeaders; };
    const Marker kMarkers[] = {
        { L"wp-content",            L"WordPress",        false },
        { L"wp-includes",           L"WordPress",        false },
        { L"/sites/default/files",  L"Drupal",           false },
        { L"drupal.settings",       L"Drupal",           false },
        { L"ng-version",            L"Angular",          false },
        { L"data-reactroot",        L"React",            false },
        { L"__next",                L"Next.js",          false },
        { L"cdn.shopify.com",       L"Shopify",          false },
        { L"laravel_session",       L"Laravel",          true  },
        { L"x-powered-by: express", L"Express.js",       true  },
        { L"x-powered-by: php",     L"PHP",              true  },
        { L"x-powered-by: asp.net", L"ASP.NET",          true  },
        { L"x-drupal-cache",        L"Drupal",           true  },
        { L"x-generator: wordpress",L"WordPress",        true  },
        { L"server: cloudflare",    L"Cloudflare (CDN/WAF)", true },
        { L"server: nginx",         L"nginx",            true  },
        { L"server: apache",        L"Apache",           true  },
        { L"server: microsoft-iis", L"Microsoft IIS",    true  },
        { L"jquery",                L"jQuery",           false },
        { L"bootstrap",             L"Bootstrap",        false },
    };
}

void TechFingerprintDialog::RegisterClassOnce() {
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

void TechFingerprintDialog::ShowFor(HWND owner) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - Tech Fingerprint",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 520, 420,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(hwnd, SW_SHOW);
}

void TechFingerprintDialog::RunScan(HWND hwnd) {
    Controls* c = GetControls(hwnd);
    std::wstring url = GetText(c->url);
    SetWindowTextW(c->results, L"Fetching...");

    HttpFetchResult r = HttpFetchGet(url);
    std::wstringstream out;
    if (!r.ok) {
        out << L"Request failed: " << r.error;
    } else {
        std::wstring headersLower = Lower(r.rawHeaders);
        std::wstring bodyLower = Lower(FromUtf8(r.body));

        std::vector<std::wstring> found;
        for (auto& m : kMarkers) {
            const std::wstring& haystack = m.inHeaders ? headersLower : bodyLower;
            if (haystack.find(m.needle) != std::wstring::npos) {
                std::wstring tech = m.tech;
                if (std::find(found.begin(), found.end(), tech) == found.end()) found.push_back(tech);
            }
        }

        out << L"HTTP " << r.statusCode << L"\r\n\r\n";
        if (found.empty()) {
            out << L"No markers matched from the built-in checklist.";
        } else {
            out << L"Detected (pattern match, not certain):\r\n";
            for (auto& t : found) out << L"  - " << t << L"\r\n";
        }
    }
    SetWindowTextW(c->results, out.str().c_str());
}

LRESULT CALLBACK TechFingerprintDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            Controls* c = new Controls();
            CreateWindowW(L"STATIC", L"URL:", WS_CHILD | WS_VISIBLE, 10, 10, 80, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            c->url = CreateWindowW(L"EDIT", L"https://", WS_CHILD | WS_VISIBLE | WS_BORDER,
                10, 30, 380, 24, hwnd, (HMENU)(INT_PTR)ID_URL, GetModuleHandleW(nullptr), nullptr);
            CreateWindowW(L"BUTTON", L"Scan", WS_CHILD | WS_VISIBLE, 400, 30, 100, 24,
                hwnd, (HMENU)(INT_PTR)ID_SCAN, GetModuleHandleW(nullptr), nullptr);
            c->results = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL, 10, 64, 490, 320,
                hwnd, (HMENU)(INT_PTR)ID_RESULTS, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HWND kids[] = { c->url, c->results };
            for (auto k : kids) SendMessageW(k, WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)c);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_SCAN && HIWORD(wParam) == BN_CLICKED) RunScan(hwnd);
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
