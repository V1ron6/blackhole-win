#include "PortScannerDialog.h"
#include "ConcurrentScan.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

namespace {
    constexpr int ID_HOST = 3301, ID_AUTH = 3302, ID_START = 3303, ID_STATUS = 3304, ID_RESULTS = 3305;
    constexpr UINT WM_SCAN_STATUS = WM_APP + 1;
    constexpr UINT WM_SCAN_DONE   = WM_APP + 2;
    constexpr int kConnectTimeoutMs = 600;
    constexpr size_t kMaxConcurrency = 12;
    const wchar_t* kClassName = L"BlackholePortScanner";

    // Same 29-port list PortScanner.kt used.
    const int kCommonPorts[] = {
        21, 22, 23, 25, 53, 80, 110, 111, 135, 139, 143, 443, 445,
        587, 993, 995, 1433, 1521, 3000, 3306, 3389, 5432, 5900,
        6379, 8000, 8080, 8443, 9200, 27017
    };

    struct State {
        HWND host, authCheck, start, status, results;
        std::shared_ptr<std::atomic<bool>> cancelled = std::make_shared<std::atomic<bool>>(false);
        bool running = false;
    };
    State* GetState(HWND h) { return reinterpret_cast<State*>(GetWindowLongPtr(h, GWLP_USERDATA)); }

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

    // Non-blocking connect + select() with a timeout - Windows has no
    // direct "connect timeout" socket option, so this is the standard way
    // to bound connect()'s wait time.
    bool TryConnect(const std::string& host, int port, int timeoutMs) {
        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* res = nullptr;
        if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0 || !res) return false;

        SOCKET sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sock == INVALID_SOCKET) { freeaddrinfo(res); return false; }

        u_long mode = 1;
        ioctlsocket(sock, FIONBIO, &mode);
        bool connected = false;
        int rc = connect(sock, res->ai_addr, (int)res->ai_addrlen);
        if (rc == 0) {
            connected = true;
        } else if (WSAGetLastError() == WSAEWOULDBLOCK) {
            fd_set writeSet, exceptSet;
            FD_ZERO(&writeSet); FD_SET(sock, &writeSet);
            FD_ZERO(&exceptSet); FD_SET(sock, &exceptSet);
            timeval tv{ timeoutMs / 1000, (timeoutMs % 1000) * 1000 };
            int sel = select(0, nullptr, &writeSet, &exceptSet, &tv);
            if (sel > 0 && FD_ISSET(sock, &writeSet) && !FD_ISSET(sock, &exceptSet)) {
                int err = 0; int errLen = sizeof(err);
                getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err, &errLen);
                connected = (err == 0);
            }
        }
        freeaddrinfo(res);
        closesocket(sock);
        return connected;
    }
}

void PortScannerDialog::RegisterClassOnce() {
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

void PortScannerDialog::ShowFor(HWND owner) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - Port Scanner",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 520, 460,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(hwnd, SW_SHOW);
}

void PortScannerDialog::StartScan(HWND hwnd) {
    State* st = GetState(hwnd);
    if (st->running) return; // Scan button is disabled while running, but guard anyway
    std::wstring host = GetText(st->host);
    if (host.empty()) { SetWindowTextW(st->status, L"Enter a host first."); return; }
    if (SendMessageW(st->authCheck, BM_GETCHECK, 0, 0) != BST_CHECKED) return;

    st->running = true;
    EnableWindow(st->start, FALSE);
    SetWindowTextW(st->results, L"");
    SetWindowTextW(st->status, (L"Scanning " + std::to_wstring(sizeof(kCommonPorts) / sizeof(int)) +
        L" ports on " + host + L"\u2026").c_str());

    *st->cancelled = false;
    auto cancelled = st->cancelled;
    std::string hostUtf8 = ToUtf8(host);

    std::thread([hwnd, cancelled, hostUtf8]() {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);

        std::mutex openMutex;
        std::vector<int> open;
        std::vector<int> ports(std::begin(kCommonPorts), std::end(kCommonPorts));

        RunConcurrent(ports, kMaxConcurrency, *cancelled, [&](int port) {
            if (cancelled->load()) return;
            if (TryConnect(hostUtf8, port, kConnectTimeoutMs)) {
                std::lock_guard<std::mutex> lock(openMutex);
                open.push_back(port);
            }
        });

        WSACleanup();

        std::sort(open.begin(), open.end());
        auto* result = new std::vector<int>(open);
        PostMessageW(hwnd, WM_SCAN_DONE, 0, (LPARAM)result);
    }).detach();
}

LRESULT CALLBACK PortScannerDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            State* st = new State();
            CreateWindowW(L"STATIC", L"Host:", WS_CHILD | WS_VISIBLE, 10, 10, 80, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            st->host = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
                10, 30, 480, 24, hwnd, (HMENU)(INT_PTR)ID_HOST, GetModuleHandleW(nullptr), nullptr);
            st->authCheck = CreateWindowW(L"BUTTON", L"I am authorized to scan this host",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 10, 64, 300, 22,
                hwnd, (HMENU)(INT_PTR)ID_AUTH, GetModuleHandleW(nullptr), nullptr);
            st->start = CreateWindowW(L"BUTTON", L"Start Scan", WS_CHILD | WS_VISIBLE | WS_DISABLED,
                10, 94, 120, 28, hwnd, (HMENU)(INT_PTR)ID_START, GetModuleHandleW(nullptr), nullptr);
            st->status = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 10, 130, 480, 20,
                hwnd, (HMENU)(INT_PTR)ID_STATUS, GetModuleHandleW(nullptr), nullptr);
            st->results = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL, 10, 154, 480, 250,
                hwnd, (HMENU)(INT_PTR)ID_RESULTS, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HWND kids[] = { st->host, st->authCheck, st->start, st->status, st->results };
            for (auto k : kids) SendMessageW(k, WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)st);
            return 0;
        }
        case WM_COMMAND: {
            State* st = GetState(hwnd);
            if (LOWORD(wParam) == ID_AUTH && HIWORD(wParam) == BN_CLICKED) {
                bool checked = SendMessageW(st->authCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
                EnableWindow(st->start, checked && !st->running);
                return 0;
            }
            if (LOWORD(wParam) == ID_START && HIWORD(wParam) == BN_CLICKED) {
                StartScan(hwnd);
                return 0;
            }
            return 0;
        }
        case WM_SCAN_DONE: {
            State* st = GetState(hwnd);
            std::unique_ptr<std::vector<int>> open((std::vector<int>*)lParam);
            st->running = false;
            bool checked = SendMessageW(st->authCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
            EnableWindow(st->start, checked);
            SetWindowTextW(st->status, (L"Done - " + std::to_wstring(open->size()) + L" open port(s) found").c_str());
            if (open->empty()) {
                SetWindowTextW(st->results, L"(none of the scanned ports responded)");
            } else {
                std::wstring text;
                for (int p : *open) text += std::to_wstring(p) + L"/tcp open\r\n";
                SetWindowTextW(st->results, text.c_str());
            }
            return 0;
        }
        case WM_DESTROY: {
            State* st = GetState(hwnd);
            *st->cancelled = true; // background thread holds its own shared_ptr, safe after this delete
            delete st;
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
