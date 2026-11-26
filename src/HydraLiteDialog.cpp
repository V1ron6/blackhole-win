#include "HydraLiteDialog.h"
#include "ConcurrentScan.h"
#include "WordlistLoader.h"
#include "SshClient.h"
#include <winhttp.h>
#include <wincrypt.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include <utility>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <sstream>
#include <cwctype>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "ws2_32.lib")

namespace {
    constexpr int ID_PROTOCOL = 3701, ID_TARGET = 3702, ID_PORT = 3703,
                  ID_USERFIELD = 3704, ID_PASSFIELD = 3705, ID_FAILIND = 3706,
                  ID_USERS_BUILTIN = 3707, ID_USERS_CUSTOM = 3708, ID_USERS_BOX = 3709,
                  ID_PASS_BUILTIN = 3710, ID_PASS_CUSTOM = 3711, ID_PASS_BOX = 3712,
                  ID_AUTH = 3713, ID_START = 3714, ID_STATUS = 3715, ID_RESULT = 3716;
    constexpr UINT WM_HYDRA_PROGRESS = WM_APP + 1;
    constexpr UINT WM_HYDRA_DONE     = WM_APP + 2;
    constexpr size_t kMaxTotalAttempts = 500;
    constexpr size_t kMaxConcurrency = 5;
    constexpr int kConnectTimeoutMs = 5000;
    const wchar_t* kClassName = L"BlackholeHydraLite";
    const wchar_t* kProtocols[] = { L"HTTP Basic Auth", L"HTTP POST Form", L"FTP", L"SSH" };

    struct State {
        HWND protocol, target, port, userField, passField, failIndicator,
             usersBuiltin, usersCustom, usersBox, passBuiltin, passCustom, passBox,
             authCheck, start, status, result;
        std::shared_ptr<std::atomic<bool>> running = std::make_shared<std::atomic<bool>>(false);
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
    std::wstring FromUtf8(const std::string& s) {
        if (s.empty()) return {};
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
        std::wstring w(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], len);
        return w;
    }
    std::wstring ToLowerCopy(std::wstring s) {
        for (auto& c : s) c = (wchar_t)towlower(c);
        return s;
    }

    std::wstring Base64Encode(const std::string& data) {
        DWORD outLen = 0;
        CryptBinaryToStringA((const BYTE*)data.data(), (DWORD)data.size(),
            CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &outLen);
        std::string out(outLen, '\0');
        CryptBinaryToStringA((const BYTE*)data.data(), (DWORD)data.size(),
            CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &out[0], &outLen);
        if (!out.empty() && out.back() == '\0') out.pop_back();
        std::wstring w(out.begin(), out.end());
        return w;
    }

    std::wstring UrlEncode(const std::wstring& in) {
        std::wstring out;
        for (wchar_t c : in) {
            if (iswalnum(c) || c == L'-' || c == L'_' || c == L'.' || c == L'~') out += c;
            else { wchar_t buf[8]; wsprintfW(buf, L"%%%02X", c); out += buf; }
        }
        return out;
    }

    // --- Protocol modules, same shape as HydraLite.kt's private functions ---

    bool AttemptHttpBasic(const std::wstring& urlStr, const std::wstring& user, const std::wstring& pass) {
        URL_COMPONENTS uc{}; uc.dwStructSize = sizeof(uc);
        wchar_t host[256]{}, path[2048]{};
        uc.lpszHostName = host; uc.dwHostNameLength = 255;
        uc.lpszUrlPath = path; uc.dwUrlPathLength = 2047;
        if (!WinHttpCrackUrl(urlStr.c_str(), (DWORD)urlStr.size(), 0, &uc)) return false;
        bool https = (uc.nScheme == INTERNET_SCHEME_HTTPS);

        HINTERNET hSession = WinHttpOpen(L"Blackhole/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        HINTERNET hConnect = hSession ? WinHttpConnect(hSession, host, uc.nPort, 0) : nullptr;
        HINTERNET hRequest = hConnect ? WinHttpOpenRequest(hConnect, L"GET", path[0] ? path : L"/",
            nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, https ? WINHTTP_FLAG_SECURE : 0) : nullptr;
        bool success = false;
        if (hRequest) {
            DWORD disableRedirects = WINHTTP_DISABLE_REDIRECTS;
            WinHttpSetOption(hRequest, WINHTTP_OPTION_DISABLE_FEATURE, &disableRedirects, sizeof(disableRedirects));
            WinHttpSetTimeouts(hRequest, kConnectTimeoutMs, kConnectTimeoutMs, kConnectTimeoutMs, kConnectTimeoutMs);

            std::wstring token = Base64Encode(ToUtf8(user + L":" + pass));
            std::wstring header = L"Authorization: Basic " + token;
            BOOL ok = WinHttpSendRequest(hRequest, header.c_str(), (DWORD)-1,
                WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
            if (ok) ok = WinHttpReceiveResponse(hRequest, nullptr);
            if (ok) {
                DWORD statusCode = 0, size = sizeof(statusCode);
                WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_FLAG_NUMBER | WINHTTP_QUERY_STATUS_CODE,
                    WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &size, WINHTTP_NO_HEADER_INDEX);
                success = (statusCode == 200);
            }
        }
        if (hRequest) WinHttpCloseHandle(hRequest);
        if (hConnect) WinHttpCloseHandle(hConnect);
        if (hSession) WinHttpCloseHandle(hSession);
        return success;
    }

    // Mirrors real Hydra's http-post-form idea: judge success by the
    // ABSENCE of a known failure string in the body, not by assuming a
    // success string/redirect (too site-specific to hardcode).
    bool AttemptHttpForm(const std::wstring& urlStr, const std::wstring& userField, const std::wstring& passField,
                         const std::wstring& failIndicator, const std::wstring& user, const std::wstring& pass) {
        URL_COMPONENTS uc{}; uc.dwStructSize = sizeof(uc);
        wchar_t host[256]{}, path[2048]{};
        uc.lpszHostName = host; uc.dwHostNameLength = 255;
        uc.lpszUrlPath = path; uc.dwUrlPathLength = 2047;
        if (!WinHttpCrackUrl(urlStr.c_str(), (DWORD)urlStr.size(), 0, &uc)) return false;
        bool https = (uc.nScheme == INTERNET_SCHEME_HTTPS);

        HINTERNET hSession = WinHttpOpen(L"Blackhole/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        HINTERNET hConnect = hSession ? WinHttpConnect(hSession, host, uc.nPort, 0) : nullptr;
        HINTERNET hRequest = hConnect ? WinHttpOpenRequest(hConnect, L"POST", path[0] ? path : L"/",
            nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, https ? WINHTTP_FLAG_SECURE : 0) : nullptr;
        bool success = false;
        if (hRequest) {
            DWORD disableRedirects = WINHTTP_DISABLE_REDIRECTS;
            WinHttpSetOption(hRequest, WINHTTP_OPTION_DISABLE_FEATURE, &disableRedirects, sizeof(disableRedirects));
            WinHttpSetTimeouts(hRequest, kConnectTimeoutMs, kConnectTimeoutMs, kConnectTimeoutMs, kConnectTimeoutMs);

            std::wstring body = UrlEncode(userField) + L"=" + UrlEncode(user) + L"&" +
                                 UrlEncode(passField) + L"=" + UrlEncode(pass);
            std::string bodyUtf8 = ToUtf8(body);
            std::wstring headers = L"Content-Type: application/x-www-form-urlencoded";
            BOOL ok = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)-1,
                (LPVOID)bodyUtf8.data(), (DWORD)bodyUtf8.size(), (DWORD)bodyUtf8.size(), 0);
            if (ok) ok = WinHttpReceiveResponse(hRequest, nullptr);
            if (ok) {
                if (failIndicator.empty()) {
                    DWORD statusCode = 0, size = sizeof(statusCode);
                    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_FLAG_NUMBER | WINHTTP_QUERY_STATUS_CODE,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &size, WINHTTP_NO_HEADER_INDEX);
                    success = (statusCode == 200); // no indicator given - much less reliable fallback, same as original
                } else {
                    std::string bodyOut;
                    DWORD avail = 0;
                    do {
                        avail = 0;
                        if (!WinHttpQueryDataAvailable(hRequest, &avail) || avail == 0) break;
                        if (avail > 4096) avail = 4096;
                        std::vector<char> buf(avail);
                        DWORD read = 0;
                        WinHttpReadData(hRequest, buf.data(), avail, &read);
                        bodyOut.append(buf.data(), read);
                        break; // only need the first chunk, matching the original's MAX_RESPONSE_BYTES single read
                    } while (avail > 0);
                    std::wstring bodyLower = ToLowerCopy(FromUtf8(bodyOut));
                    success = bodyLower.find(ToLowerCopy(failIndicator)) == std::wstring::npos;
                }
            }
        }
        if (hRequest) WinHttpCloseHandle(hRequest);
        if (hConnect) WinHttpCloseHandle(hConnect);
        if (hSession) WinHttpCloseHandle(hSession);
        return success;
    }

    // Minimal FTP login (USER/PASS over a raw socket) - no data channel, just auth.
    bool AttemptFtp(const std::wstring& host, int port, const std::wstring& user, const std::wstring& pass) {
        addrinfo hints{}; hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
        addrinfo* res = nullptr;
        std::string hostUtf8 = ToUtf8(host);
        if (getaddrinfo(hostUtf8.c_str(), std::to_string(port).c_str(), &hints, &res) != 0 || !res) return false;
        SOCKET sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        DWORD timeout = kConnectTimeoutMs;
        if (sock != INVALID_SOCKET) setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        if (sock == INVALID_SOCKET || connect(sock, res->ai_addr, (int)res->ai_addrlen) != 0) {
            if (sock != INVALID_SOCKET) closesocket(sock);
            freeaddrinfo(res);
            return false;
        }
        freeaddrinfo(res);

        auto readLine = [&]() -> std::string {
            std::string line; char c;
            while (recv(sock, &c, 1, 0) == 1) { if (c == '\n') break; if (c != '\r') line += c; }
            return line;
        };
        auto readCode = [&]() -> int {
            std::string line = readLine();
            if (line.size() < 3) return -1;
            try { return std::stoi(line.substr(0, 3)); } catch (...) { return -1; }
        };
        auto sendLine = [&](const std::string& cmd) {
            std::string line = cmd + "\r\n";
            send(sock, line.c_str(), (int)line.size(), 0);
        };

        readCode(); // banner (220)
        sendLine("USER " + ToUtf8(user));
        int userCode = readCode();
        bool result = false;
        if (userCode == 230) result = true;
        else if (userCode == 331) {
            sendLine("PASS " + ToUtf8(pass));
            result = (readCode() == 230);
        }
        closesocket(sock);
        return result;
    }

    bool AttemptSsh(const std::wstring& host, int port, const std::wstring& user, const std::wstring& pass) {
        if (!SshClient::Available()) return false;
        SshClient client;
        std::wstring err;
        // Hydra-lite deliberately skips host-key pinning (same as the
        // original Kotlin's PromiscuousVerifier) - this is a credential
        // test against a service you're already authorized to attack, not
        // an interactive session you'd want MITM protection on long-term.
        if (!client.Connect(host, port, kConnectTimeoutMs, err)) return false;
        bool ok = client.AuthPassword(user, pass);
        client.Disconnect();
        return ok;
    }
}

void HydraLiteDialog::RegisterClassOnce() {
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

void HydraLiteDialog::ShowFor(HWND owner) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - Hydra-lite (Credential Brute Forcer)",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 600, 560,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateFieldVisibility(hwnd);
}

void HydraLiteDialog::UpdateFieldVisibility(HWND hwnd) {
    State* st = GetState(hwnd);
    int sel = (int)SendMessageW(st->protocol, CB_GETCURSEL, 0, 0);
    std::wstring protocol = kProtocols[sel < 0 ? 0 : sel];
    bool showPort = (protocol == L"FTP" || protocol == L"SSH");
    bool showForm = (protocol == L"HTTP POST Form");
    ShowWindow(st->port, showPort ? SW_SHOW : SW_HIDE);
    ShowWindow(st->userField, showForm ? SW_SHOW : SW_HIDE);
    ShowWindow(st->passField, showForm ? SW_SHOW : SW_HIDE);
    ShowWindow(st->failIndicator, showForm ? SW_SHOW : SW_HIDE);
}

void HydraLiteDialog::ToggleAttack(HWND hwnd) {
    State* st = GetState(hwnd);
    if (st->running->load()) {
        *st->running = false;
        return;
    }
    if (SendMessageW(st->authCheck, BM_GETCHECK, 0, 0) != BST_CHECKED) return;

    std::wstring target = GetText(st->target);
    size_t a = target.find_first_not_of(L" \t");
    target = (a == std::wstring::npos) ? L"" : target.substr(a);
    if (target.empty()) { SetWindowTextW(st->status, L"Enter a target first."); return; }

    int protoIndex = (int)SendMessageW(st->protocol, CB_GETCURSEL, 0, 0);
    std::wstring protocol = kProtocols[protoIndex < 0 ? 0 : protoIndex];

    bool usersCustom = SendMessageW(st->usersCustom, BM_GETCHECK, 0, 0) == BST_CHECKED;
    std::vector<std::wstring> users = usersCustom
        ? ParseWordlistText(GetText(st->usersBox))
        : LoadWordlistFile(ExeDirectory() + L"\\assets\\common_usernames.txt");
    bool passCustom = SendMessageW(st->passCustom, BM_GETCHECK, 0, 0) == BST_CHECKED;
    std::vector<std::wstring> passwords = passCustom
        ? ParseWordlistText(GetText(st->passBox))
        : LoadWordlistFile(ExeDirectory() + L"\\assets\\common_passwords.txt");

    if (users.empty() || passwords.empty()) { SetWindowTextW(st->status, L"Username or password list is empty."); return; }

    std::vector<std::pair<std::wstring, std::wstring>> pairs;
    bool truncated = (uint64_t)users.size() * (uint64_t)passwords.size() > kMaxTotalAttempts;
    for (auto& u : users) {
        for (auto& p : passwords) {
            pairs.push_back({ u, p });
            if (pairs.size() >= kMaxTotalAttempts) break;
        }
        if (pairs.size() >= kMaxTotalAttempts) break;
    }

    int port = _wtoi(GetText(st->port).c_str());
    std::wstring userField = GetText(st->userField); if (userField.empty()) userField = L"username";
    std::wstring passField = GetText(st->passField); if (passField.empty()) passField = L"password";
    std::wstring failIndicator = GetText(st->failIndicator);

    *st->running = true;
    SetWindowTextW(st->start, L"Stop");
    SetWindowTextW(st->result, L"");
    SetWindowTextW(st->status, (truncated
        ? (L"Trying " + std::to_wstring(pairs.size()) + L" pairs (capped)\u2026")
        : (L"Trying " + std::to_wstring(pairs.size()) + L" pairs\u2026")).c_str());

    auto running = st->running;
    std::thread([hwnd, running, protocol, target, port, userField, passField, failIndicator, pairs]() {
        WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa);

        auto tried = std::make_shared<std::atomic<int>>(0);
        auto found = std::make_shared<std::atomic<bool>>(false);
        auto winner = std::make_shared<std::pair<std::wstring, std::wstring>>();
        auto winnerMutex = std::make_shared<std::mutex>();
        int total = (int)pairs.size();

        RunConcurrent(pairs, kMaxConcurrency, *running, [&](const std::pair<std::wstring, std::wstring>& cred) {
            if (!running->load() || found->load()) return;
            bool success = false;
            try {
                if (protocol == L"HTTP Basic Auth") success = AttemptHttpBasic(target, cred.first, cred.second);
                else if (protocol == L"HTTP POST Form") success = AttemptHttpForm(target, userField, passField, failIndicator, cred.first, cred.second);
                else if (protocol == L"FTP") success = AttemptFtp(target, port ? port : 21, cred.first, cred.second);
                else if (protocol == L"SSH") success = AttemptSsh(target, port ? port : 22, cred.first, cred.second);
            } catch (...) { success = false; }

            int done = tried->fetch_add(1) + 1;
            bool justFound = false;
            if (success) {
                std::lock_guard<std::mutex> lock(*winnerMutex);
                if (!found->exchange(true)) { *winner = cred; justFound = true; *running = false; }
            }
            if (done % 10 == 0 || done == total || justFound) {
                auto* status = new std::wstring(L"Tried " + std::to_wstring(done) + L"/" + std::to_wstring(total) + L"\u2026");
                PostMessageW(hwnd, WM_HYDRA_PROGRESS, 0, (LPARAM)status);
            }
        });

        WSACleanup();

        bool wasStopped = !running->load() && !found->load();
        *running = false;
        auto* result = new std::pair<std::pair<std::wstring, std::wstring>, int>(
            found->load() ? *winner : std::pair<std::wstring, std::wstring>{}, tried->load());
        PostMessageW(hwnd, WM_HYDRA_DONE, wasStopped ? 1 : 0, (LPARAM)result);
    }).detach();
}

LRESULT CALLBACK HydraLiteDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            State* st = new State();
            CreateWindowW(L"STATIC", L"Protocol:", WS_CHILD | WS_VISIBLE, 10, 10, 70, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            st->protocol = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                90, 8, 160, 100, hwnd, (HMENU)(INT_PTR)ID_PROTOCOL, GetModuleHandleW(nullptr), nullptr);
            for (auto p : kProtocols) SendMessageW(st->protocol, CB_ADDSTRING, 0, (LPARAM)p);
            SendMessageW(st->protocol, CB_SETCURSEL, 0, 0);

            CreateWindowW(L"STATIC", L"Target:", WS_CHILD | WS_VISIBLE, 260, 10, 60, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            st->target = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
                320, 8, 180, 24, hwnd, (HMENU)(INT_PTR)ID_TARGET, GetModuleHandleW(nullptr), nullptr);
            st->port = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_BORDER | ES_NUMBER,
                510, 8, 60, 24, hwnd, (HMENU)(INT_PTR)ID_PORT, GetModuleHandleW(nullptr), nullptr);

            st->userField = CreateWindowW(L"EDIT", L"username (form field name)", WS_CHILD | WS_BORDER,
                10, 40, 180, 24, hwnd, (HMENU)(INT_PTR)ID_USERFIELD, GetModuleHandleW(nullptr), nullptr);
            st->passField = CreateWindowW(L"EDIT", L"password (form field name)", WS_CHILD | WS_BORDER,
                200, 40, 180, 24, hwnd, (HMENU)(INT_PTR)ID_PASSFIELD, GetModuleHandleW(nullptr), nullptr);
            st->failIndicator = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_BORDER,
                390, 40, 180, 24, hwnd, (HMENU)(INT_PTR)ID_FAILIND, GetModuleHandleW(nullptr), nullptr);

            st->usersBuiltin = CreateWindowW(L"BUTTON", L"Built-in usernames", WS_CHILD | WS_VISIBLE |
                BS_AUTORADIOBUTTON | WS_GROUP, 10, 72, 150, 20,
                hwnd, (HMENU)(INT_PTR)ID_USERS_BUILTIN, GetModuleHandleW(nullptr), nullptr);
            st->usersCustom = CreateWindowW(L"BUTTON", L"Custom usernames", WS_CHILD | WS_VISIBLE |
                BS_AUTORADIOBUTTON, 170, 72, 150, 20,
                hwnd, (HMENU)(INT_PTR)ID_USERS_CUSTOM, GetModuleHandleW(nullptr), nullptr);
            SendMessageW(st->usersBuiltin, BM_SETCHECK, BST_CHECKED, 0);
            st->usersBox = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_BORDER | ES_MULTILINE | WS_VSCROLL,
                10, 94, 280, 50, hwnd, (HMENU)(INT_PTR)ID_USERS_BOX, GetModuleHandleW(nullptr), nullptr);

            st->passBuiltin = CreateWindowW(L"BUTTON", L"Built-in passwords", WS_CHILD | WS_VISIBLE |
                BS_AUTORADIOBUTTON | WS_GROUP, 300, 72, 150, 20,
                hwnd, (HMENU)(INT_PTR)ID_PASS_BUILTIN, GetModuleHandleW(nullptr), nullptr);
            st->passCustom = CreateWindowW(L"BUTTON", L"Custom passwords", WS_CHILD | WS_VISIBLE |
                BS_AUTORADIOBUTTON, 460, 72, 130, 20,
                hwnd, (HMENU)(INT_PTR)ID_PASS_CUSTOM, GetModuleHandleW(nullptr), nullptr);
            SendMessageW(st->passBuiltin, BM_SETCHECK, BST_CHECKED, 0);
            st->passBox = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_BORDER | ES_MULTILINE | WS_VSCROLL,
                300, 94, 290, 50, hwnd, (HMENU)(INT_PTR)ID_PASS_BOX, GetModuleHandleW(nullptr), nullptr);

            st->authCheck = CreateWindowW(L"BUTTON", L"I am authorized to attack this target",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 10, 152, 320, 22,
                hwnd, (HMENU)(INT_PTR)ID_AUTH, GetModuleHandleW(nullptr), nullptr);
            st->start = CreateWindowW(L"BUTTON", L"Start", WS_CHILD | WS_VISIBLE | WS_DISABLED,
                10, 180, 100, 28, hwnd, (HMENU)(INT_PTR)ID_START, GetModuleHandleW(nullptr), nullptr);
            st->status = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 10, 216, 560, 20,
                hwnd, (HMENU)(INT_PTR)ID_STATUS, GetModuleHandleW(nullptr), nullptr);
            st->result = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY,
                10, 240, 560, 24, hwnd, (HMENU)(INT_PTR)ID_RESULT, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HWND kids[] = { st->protocol, st->target, st->port, st->userField, st->passField, st->failIndicator,
                             st->usersBuiltin, st->usersCustom, st->usersBox, st->passBuiltin, st->passCustom,
                             st->passBox, st->authCheck, st->start, st->status, st->result };
            for (auto k : kids) SendMessageW(k, WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)st);
            return 0;
        }
        case WM_COMMAND: {
            State* st = GetState(hwnd);
            if (LOWORD(wParam) == ID_PROTOCOL && HIWORD(wParam) == CBN_SELCHANGE) {
                HydraLiteDialog::UpdateFieldVisibility(hwnd);
                return 0;
            }
            if ((LOWORD(wParam) == ID_USERS_BUILTIN || LOWORD(wParam) == ID_USERS_CUSTOM) && HIWORD(wParam) == BN_CLICKED) {
                bool custom = SendMessageW(st->usersCustom, BM_GETCHECK, 0, 0) == BST_CHECKED;
                ShowWindow(st->usersBox, custom ? SW_SHOW : SW_HIDE);
                return 0;
            }
            if ((LOWORD(wParam) == ID_PASS_BUILTIN || LOWORD(wParam) == ID_PASS_CUSTOM) && HIWORD(wParam) == BN_CLICKED) {
                bool custom = SendMessageW(st->passCustom, BM_GETCHECK, 0, 0) == BST_CHECKED;
                ShowWindow(st->passBox, custom ? SW_SHOW : SW_HIDE);
                return 0;
            }
            if (LOWORD(wParam) == ID_AUTH && HIWORD(wParam) == BN_CLICKED) {
                bool checked = SendMessageW(st->authCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
                EnableWindow(st->start, checked);
                return 0;
            }
            if (LOWORD(wParam) == ID_START && HIWORD(wParam) == BN_CLICKED) {
                HydraLiteDialog::ToggleAttack(hwnd);
                return 0;
            }
            return 0;
        }
        case WM_HYDRA_PROGRESS: {
            State* st = GetState(hwnd);
            std::unique_ptr<std::wstring> text((std::wstring*)lParam);
            SetWindowTextW(st->status, text->c_str());
            return 0;
        }
        case WM_HYDRA_DONE: {
            State* st = GetState(hwnd);
            std::unique_ptr<std::pair<std::pair<std::wstring, std::wstring>, int>> r(
                (std::pair<std::pair<std::wstring, std::wstring>, int>*)lParam);
            bool wasStopped = (wParam == 1);
            SetWindowTextW(st->start, L"Start");
            if (!r->first.first.empty()) {
                SetWindowTextW(st->status, (L"Found after " + std::to_wstring(r->second) + L" attempt(s).").c_str());
                SetWindowTextW(st->result, (r->first.first + L" : " + r->first.second).c_str());
            } else if (wasStopped) {
                SetWindowTextW(st->status, (L"Stopped after " + std::to_wstring(r->second) + L" attempt(s).").c_str());
            } else {
                SetWindowTextW(st->status, (L"No working credentials found in " + std::to_wstring(r->second) + L" attempt(s).").c_str());
            }
            return 0;
        }
        case WM_DESTROY: {
            State* st = GetState(hwnd);
            *st->running = false;
            delete st;
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
