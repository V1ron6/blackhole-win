#include "WhoisLookupDialog.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

namespace {
    constexpr int ID_DOMAIN = 1201, ID_SERVER = 1202, ID_QUERY = 1203, ID_RESULTS = 1204;
    const wchar_t* kClassName = L"BlackholeWhoisLookup";

    struct Controls { HWND domain, server, results; };
    Controls* GetControls(HWND h) { return reinterpret_cast<Controls*>(GetWindowLongPtr(h, GWLP_USERDATA)); }

    std::wstring GetText(HWND h) {
        int len = GetWindowTextLengthW(h);
        std::wstring s(len, L'\0');
        if (len > 0) GetWindowTextW(h, &s[0], len + 1);
        return s;
    }

    std::string ToUtf8(const std::wstring& w) {
        if (w.empty()) return {};
        int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
        std::string s(len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], len, nullptr, nullptr);
        return s;
    }

    std::wstring FromUtf8(const std::string& s) {
        if (s.empty()) return {};
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
        std::wstring w(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], len);
        return w;
    }

    // Plain-text WHOIS protocol (RFC 3912): connect to port 43, send
    // "<query>\r\n", read until the server closes the connection.
    std::wstring WhoisQuery(const std::wstring& server, const std::wstring& domain, std::wstring& err) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { err = L"WSAStartup failed."; return L""; }

        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* result = nullptr;
        std::string serverUtf8 = ToUtf8(server);
        if (getaddrinfo(serverUtf8.c_str(), "43", &hints, &result) != 0 || !result) {
            err = L"Could not resolve WHOIS server: " + server;
            WSACleanup();
            return L"";
        }

        SOCKET sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (sock == INVALID_SOCKET || connect(sock, result->ai_addr, (int)result->ai_addrlen) != 0) {
            err = L"Could not connect to " + server + L":43";
            if (sock != INVALID_SOCKET) closesocket(sock);
            freeaddrinfo(result);
            WSACleanup();
            return L"";
        }
        freeaddrinfo(result);

        // 10-second receive timeout so a non-responsive server doesn't hang the UI thread.
        DWORD timeoutMs = 10000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs, sizeof(timeoutMs));

        std::string query = ToUtf8(domain) + "\r\n";
        send(sock, query.c_str(), (int)query.size(), 0);

        std::string response;
        char buf[4096];
        int n;
        while ((n = recv(sock, buf, sizeof(buf), 0)) > 0) {
            response.append(buf, n);
        }
        closesocket(sock);
        WSACleanup();

        if (response.empty()) { err = L"No response (connection closed with no data)."; return L""; }
        return FromUtf8(response);
    }
}

void WhoisLookupDialog::RegisterClassOnce() {
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

void WhoisLookupDialog::ShowFor(HWND owner) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - WHOIS Lookup",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 560, 480,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(hwnd, SW_SHOW);
}

void WhoisLookupDialog::RunQuery(HWND hwnd) {
    Controls* c = GetControls(hwnd);
    std::wstring domain = GetText(c->domain);
    std::wstring server = GetText(c->server);
    if (domain.empty()) return;
    if (server.empty()) server = L"whois.iana.org";

    SetWindowTextW(c->results, L"Querying...");
    std::wstring err;
    std::wstring response = WhoisQuery(server, domain, err);
    SetWindowTextW(c->results, err.empty() ? response.c_str() : err.c_str());
}

LRESULT CALLBACK WhoisLookupDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            Controls* c = new Controls();
            CreateWindowW(L"STATIC", L"Domain:", WS_CHILD | WS_VISIBLE, 10, 10, 80, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            c->domain = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
                10, 30, 300, 24, hwnd, (HMENU)(INT_PTR)ID_DOMAIN, GetModuleHandleW(nullptr), nullptr);

            CreateWindowW(L"STATIC", L"WHOIS server:", WS_CHILD | WS_VISIBLE, 320, 10, 100, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            c->server = CreateWindowW(L"EDIT", L"whois.iana.org", WS_CHILD | WS_VISIBLE | WS_BORDER,
                320, 30, 150, 24, hwnd, (HMENU)(INT_PTR)ID_SERVER, GetModuleHandleW(nullptr), nullptr);

            CreateWindowW(L"BUTTON", L"Query", WS_CHILD | WS_VISIBLE, 10, 64, 100, 26,
                hwnd, (HMENU)(INT_PTR)ID_QUERY, GetModuleHandleW(nullptr), nullptr);
            CreateWindowW(L"STATIC", L"Tip: if IANA's reply shows a \"refer:\" / registry WHOIS server, paste it above and query again for full details.",
                WS_CHILD | WS_VISIBLE, 120, 68, 400, 32, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);

            c->results = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_HSCROLL, 10, 104, 520, 320,
                hwnd, (HMENU)(INT_PTR)ID_RESULTS, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HWND kids[] = { c->domain, c->server, c->results };
            for (auto k : kids) SendMessageW(k, WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)c);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_QUERY && HIWORD(wParam) == BN_CLICKED) RunQuery(hwnd);
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
