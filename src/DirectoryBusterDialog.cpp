#include "DirectoryBusterDialog.h"
#include "ConcurrentScan.h"
#include "WordlistLoader.h"
#include <winhttp.h>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

namespace {
    constexpr int ID_HOST = 3401, ID_WORDLIST_BUILTIN = 3402, ID_WORDLIST_CUSTOM = 3403,
                  ID_CUSTOM_BOX = 3404, ID_EXTENSIONS = 3405, ID_AUTH = 3406, ID_START = 3407,
                  ID_STATUS = 3408, ID_RESULTS = 3409;
    constexpr UINT WM_BUST_PROGRESS = WM_APP + 1;
    constexpr UINT WM_BUST_HIT      = WM_APP + 2;
    constexpr UINT WM_BUST_DONE     = WM_APP + 3;
    constexpr size_t kMaxTotalRequests = 2000;
    constexpr size_t kMaxConcurrency = 8;
    constexpr int kConnectTimeoutMs = 4000;
    const wchar_t* kClassName = L"BlackholeDirectoryBuster";

    struct State {
        HWND host, wordlistBuiltin, wordlistCustom, customBox, extensions, authCheck, start, status, results;
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

    std::vector<std::wstring> SplitCommaList(const std::wstring& s) {
        std::vector<std::wstring> out;
        std::wstringstream ss(s);
        std::wstring part;
        while (std::getline(ss, part, L',')) {
            size_t a = part.find_first_not_of(L" \t");
            if (a == std::wstring::npos) continue;
            size_t b = part.find_last_not_of(L" \t");
            std::wstring trimmed = part.substr(a, b - a + 1);
            if (!trimmed.empty() && trimmed.front() == L'.') trimmed = trimmed.substr(1);
            if (!trimmed.empty()) out.push_back(trimmed);
        }
        return out;
    }

    std::vector<std::wstring> BuildCandidateList(const std::vector<std::wstring>& words,
                                                  const std::vector<std::wstring>& extensions) {
        if (extensions.empty()) return words;
        std::vector<std::wstring> result;
        for (auto& w : words) {
            result.push_back(w);
            for (auto& ext : extensions) result.push_back(w + L"." + ext);
        }
        return result;
    }

    // Probes baseUrl/path, reading only the status line - never the body,
    // never following redirects (a 3xx is itself a meaningful signal here).
    // Returns -1 for a 404 or a failed request (both treated as "not found").
    int Probe(const std::wstring& baseUrl, const std::wstring& path) {
        std::wstring url = baseUrl + L"/" + path;
        URL_COMPONENTS uc{};
        uc.dwStructSize = sizeof(uc);
        wchar_t host[256]{}, urlPath[2048]{};
        uc.lpszHostName = host; uc.dwHostNameLength = 255;
        uc.lpszUrlPath = urlPath; uc.dwUrlPathLength = 2047;
        if (!WinHttpCrackUrl(url.c_str(), (DWORD)url.size(), 0, &uc)) return -1;
        bool https = (uc.nScheme == INTERNET_SCHEME_HTTPS);

        HINTERNET hSession = WinHttpOpen(L"Blackhole/1.0 (Windows port)",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return -1;
        HINTERNET hConnect = WinHttpConnect(hSession, host, uc.nPort, 0);
        if (!hConnect) { WinHttpCloseHandle(hSession); return -1; }
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", urlPath[0] ? urlPath : L"/",
            nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, https ? WINHTTP_FLAG_SECURE : 0);
        if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return -1; }

        DWORD disableRedirects = WINHTTP_DISABLE_REDIRECTS;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_DISABLE_FEATURE, &disableRedirects, sizeof(disableRedirects));
        WinHttpSetTimeouts(hRequest, kConnectTimeoutMs, kConnectTimeoutMs, kConnectTimeoutMs, kConnectTimeoutMs);

        int result = -1;
        BOOL ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
        if (ok) ok = WinHttpReceiveResponse(hRequest, nullptr);
        if (ok) {
            DWORD statusCode = 0, size = sizeof(statusCode);
            WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_FLAG_NUMBER | WINHTTP_QUERY_STATUS_CODE,
                WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &size, WINHTTP_NO_HEADER_INDEX);
            if (statusCode != 404) result = (int)statusCode;
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }
}

void DirectoryBusterDialog::RegisterClassOnce() {
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

void DirectoryBusterDialog::ShowFor(HWND owner) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - Directory Buster",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 560, 560,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(hwnd, SW_SHOW);
}

void DirectoryBusterDialog::StartScan(HWND hwnd) {
    State* st = GetState(hwnd);
    if (st->running) return;
    if (SendMessageW(st->authCheck, BM_GETCHECK, 0, 0) != BST_CHECKED) return;

    std::wstring baseUrl = GetText(st->host);
    size_t end = baseUrl.find_last_not_of(L'/');
    baseUrl = (end == std::wstring::npos) ? baseUrl : baseUrl.substr(0, end + 1);
    if (baseUrl.empty()) { SetWindowTextW(st->status, L"Enter a target URL first."); return; }

    bool useCustom = SendMessageW(st->wordlistCustom, BM_GETCHECK, 0, 0) == BST_CHECKED;
    std::vector<std::wstring> words = useCustom
        ? ParseWordlistText(GetText(st->customBox))
        : LoadWordlistFile(ExeDirectory() + L"\\assets\\common_paths.txt");
    if (words.empty()) { SetWindowTextW(st->status, L"Wordlist is empty."); return; }

    auto extensions = SplitCommaList(GetText(st->extensions));
    auto candidates = BuildCandidateList(words, extensions);
    bool truncated = candidates.size() > kMaxTotalRequests;
    if (truncated) candidates.resize(kMaxTotalRequests);

    st->running = true;
    EnableWindow(st->start, FALSE);
    SetWindowTextW(st->results, L"");
    SetWindowTextW(st->status, (truncated
        ? (L"Scanning " + std::to_wstring(candidates.size()) + L" paths (truncated) on " + baseUrl + L"\u2026")
        : (L"Scanning " + std::to_wstring(candidates.size()) + L" paths on " + baseUrl + L"\u2026")).c_str());

    *st->cancelled = false;
    auto cancelled = st->cancelled;

    std::thread([hwnd, cancelled, baseUrl, candidates]() {
        auto scanned = std::make_shared<std::atomic<int>>(0);
        auto hits = std::make_shared<std::atomic<int>>(0);
        int total = (int)candidates.size();

        RunConcurrent(candidates, kMaxConcurrency, *cancelled, [&](const std::wstring& path) {
            if (cancelled->load()) return;
            int code = Probe(baseUrl, path);
            int done = scanned->fetch_add(1) + 1;
            if (code >= 0) {
                hits->fetch_add(1);
                auto* line = new std::wstring(std::to_wstring(code) + L"  /" + path);
                PostMessageW(hwnd, WM_BUST_HIT, 0, (LPARAM)line);
            }
            if (done % 25 == 0 || done == total) {
                auto* status = new std::wstring(L"Scanned " + std::to_wstring(done) + L"/" +
                    std::to_wstring(total) + L" \u2022 " + std::to_wstring(hits->load()) + L" hit(s)");
                PostMessageW(hwnd, WM_BUST_PROGRESS, 0, (LPARAM)status);
            }
        });

        auto* summary = new std::wstring(L"Done - " + std::to_wstring(hits->load()) + L" hit(s) out of " +
            std::to_wstring(scanned->load()) + L" scanned");
        PostMessageW(hwnd, WM_BUST_DONE, 0, (LPARAM)summary);
    }).detach();
}

LRESULT CALLBACK DirectoryBusterDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            State* st = new State();
            CreateWindowW(L"STATIC", L"Target URL:", WS_CHILD | WS_VISIBLE, 10, 10, 150, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            st->host = CreateWindowW(L"EDIT", L"https://", WS_CHILD | WS_VISIBLE | WS_BORDER,
                10, 30, 520, 24, hwnd, (HMENU)(INT_PTR)ID_HOST, GetModuleHandleW(nullptr), nullptr);

            st->wordlistBuiltin = CreateWindowW(L"BUTTON", L"Built-in wordlist (~185 paths)",
                WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP, 10, 62, 250, 20,
                hwnd, (HMENU)(INT_PTR)ID_WORDLIST_BUILTIN, GetModuleHandleW(nullptr), nullptr);
            st->wordlistCustom = CreateWindowW(L"BUTTON", L"Custom wordlist (one path per line)",
                WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 270, 62, 260, 20,
                hwnd, (HMENU)(INT_PTR)ID_WORDLIST_CUSTOM, GetModuleHandleW(nullptr), nullptr);
            SendMessageW(st->wordlistBuiltin, BM_SETCHECK, BST_CHECKED, 0);
            st->customBox = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_BORDER | ES_MULTILINE | WS_VSCROLL,
                10, 86, 520, 80, hwnd, (HMENU)(INT_PTR)ID_CUSTOM_BOX, GetModuleHandleW(nullptr), nullptr);

            CreateWindowW(L"STATIC", L"Extensions (comma-separated, e.g. php,bak):", WS_CHILD | WS_VISIBLE,
                10, 172, 320, 18, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            st->extensions = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
                10, 192, 300, 24, hwnd, (HMENU)(INT_PTR)ID_EXTENSIONS, GetModuleHandleW(nullptr), nullptr);

            st->authCheck = CreateWindowW(L"BUTTON", L"I am authorized to scan this host",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 10, 224, 300, 22,
                hwnd, (HMENU)(INT_PTR)ID_AUTH, GetModuleHandleW(nullptr), nullptr);
            st->start = CreateWindowW(L"BUTTON", L"Start Scan", WS_CHILD | WS_VISIBLE | WS_DISABLED,
                10, 252, 120, 28, hwnd, (HMENU)(INT_PTR)ID_START, GetModuleHandleW(nullptr), nullptr);
            st->status = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 10, 288, 520, 20,
                hwnd, (HMENU)(INT_PTR)ID_STATUS, GetModuleHandleW(nullptr), nullptr);
            st->results = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL, 10, 312, 520, 200,
                hwnd, (HMENU)(INT_PTR)ID_RESULTS, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HWND kids[] = { st->host, st->wordlistBuiltin, st->wordlistCustom, st->customBox,
                             st->extensions, st->authCheck, st->start, st->status, st->results };
            for (auto k : kids) SendMessageW(k, WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)st);
            return 0;
        }
        case WM_COMMAND: {
            State* st = GetState(hwnd);
            if ((LOWORD(wParam) == ID_WORDLIST_BUILTIN || LOWORD(wParam) == ID_WORDLIST_CUSTOM) &&
                HIWORD(wParam) == BN_CLICKED) {
                bool custom = SendMessageW(st->wordlistCustom, BM_GETCHECK, 0, 0) == BST_CHECKED;
                ShowWindow(st->customBox, custom ? SW_SHOW : SW_HIDE);
                return 0;
            }
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
        case WM_BUST_PROGRESS: {
            State* st = GetState(hwnd);
            std::unique_ptr<std::wstring> text((std::wstring*)lParam);
            SetWindowTextW(st->status, text->c_str());
            return 0;
        }
        case WM_BUST_HIT: {
            State* st = GetState(hwnd);
            std::unique_ptr<std::wstring> line((std::wstring*)lParam);
            int len = GetWindowTextLengthW(st->results);
            SendMessageW(st->results, EM_SETSEL, len, len);
            SendMessageW(st->results, EM_REPLACESEL, FALSE, (LPARAM)(*line + L"\r\n").c_str());
            return 0;
        }
        case WM_BUST_DONE: {
            State* st = GetState(hwnd);
            std::unique_ptr<std::wstring> summary((std::wstring*)lParam);
            st->running = false;
            bool checked = SendMessageW(st->authCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
            EnableWindow(st->start, checked);
            SetWindowTextW(st->status, summary->c_str());
            return 0;
        }
        case WM_DESTROY: {
            State* st = GetState(hwnd);
            *st->cancelled = true;
            delete st;
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
