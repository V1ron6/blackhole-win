#include "RobotsFetchDialog.h"
#include "HttpFetch.h"
#include <string>

namespace {
    constexpr int ID_URL = 1501, ID_FETCH = 1502, ID_RESULTS = 1503;
    const wchar_t* kClassName = L"BlackholeRobotsFetch";

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

    // Accepts either a bare host or a full URL; always normalizes to
    // "<scheme>://<host>/robots.txt" regardless of what path was typed.
    std::wstring RobotsUrlFor(std::wstring input) {
        if (input.find(L"://") == std::wstring::npos) input = L"https://" + input;
        size_t schemeEnd = input.find(L"://") + 3;
        size_t pathStart = input.find(L'/', schemeEnd);
        std::wstring origin = (pathStart == std::wstring::npos) ? input : input.substr(0, pathStart);
        return origin + L"/robots.txt";
    }
}

void RobotsFetchDialog::RegisterClassOnce() {
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

void RobotsFetchDialog::ShowFor(HWND owner) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - robots.txt Fetch",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 520, 440,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(hwnd, SW_SHOW);
}

void RobotsFetchDialog::Fetch(HWND hwnd) {
    Controls* c = GetControls(hwnd);
    std::wstring site = GetText(c->url);
    if (site.empty()) return;
    std::wstring robotsUrl = RobotsUrlFor(site);

    SetWindowTextW(c->results, (L"Fetching " + robotsUrl + L" ...").c_str());
    HttpFetchResult r = HttpFetchGet(robotsUrl);
    if (!r.ok) {
        SetWindowTextW(c->results, (L"Request failed: " + r.error).c_str());
    } else if (r.statusCode != 200) {
        SetWindowTextW(c->results, (L"HTTP " + std::to_wstring(r.statusCode) +
            L" - no robots.txt found at " + robotsUrl).c_str());
    } else {
        SetWindowTextW(c->results, FromUtf8(r.body).c_str());
    }
}

LRESULT CALLBACK RobotsFetchDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            Controls* c = new Controls();
            CreateWindowW(L"STATIC", L"Site (URL or bare host):", WS_CHILD | WS_VISIBLE, 10, 10, 250, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            c->url = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
                10, 30, 380, 24, hwnd, (HMENU)(INT_PTR)ID_URL, GetModuleHandleW(nullptr), nullptr);
            CreateWindowW(L"BUTTON", L"Fetch", WS_CHILD | WS_VISIBLE, 400, 30, 100, 24,
                hwnd, (HMENU)(INT_PTR)ID_FETCH, GetModuleHandleW(nullptr), nullptr);
            c->results = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_HSCROLL, 10, 64, 490, 330,
                hwnd, (HMENU)(INT_PTR)ID_RESULTS, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HWND kids[] = { c->url, c->results };
            for (auto k : kids) SendMessageW(k, WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)c);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_FETCH && HIWORD(wParam) == BN_CLICKED) Fetch(hwnd);
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
