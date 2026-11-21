#include "PostmanDialog.h"
#include <winhttp.h>
#include <string>
#include <vector>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

namespace {
    constexpr int ID_METHOD   = 101;
    constexpr int ID_URL      = 102;
    constexpr int ID_HEADERS  = 103;
    constexpr int ID_BODY     = 104;
    constexpr int ID_SEND     = 105;
    constexpr int ID_RESPONSE = 106;

    const wchar_t* kClassName = L"BlackholePostmanDialog";

    // Mirrors HttpMethod.kt
    const wchar_t* kMethods[] = { L"GET", L"POST", L"PUT", L"PATCH", L"DELETE" };

    struct Controls {
        HWND method, url, headers, body, send, response;
    };

    Controls* GetControls(HWND hwnd) {
        return reinterpret_cast<Controls*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    std::wstring GetText(HWND h) {
        int len = GetWindowTextLengthW(h);
        std::wstring s(len, L'\0');
        if (len > 0) GetWindowTextW(h, &s[0], len + 1);
        return s;
    }

    // Parses a "Header-Name: value" per line textbox into the single
    // CRLF-joined header block WinHTTP expects.
    std::wstring BuildHeaderBlock(const std::wstring& raw) {
        std::wstringstream out;
        std::wstringstream in(raw);
        std::wstring line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == L'\r') line.pop_back();
            if (line.find(L':') == std::wstring::npos) continue;
            out << line << L"\r\n";
        }
        return out.str();
    }
}

void PostmanDialog::RegisterClassOnce() {
    static bool registered = false;
    if (registered) return;
    WNDCLASSW wc{};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = kClassName;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);
    registered = true;
}

void PostmanDialog::ShowFor(HWND owner) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - HTTP Request Tool (Intermediate mode)",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 640, 560,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(hwnd, SW_SHOW);
}

void PostmanDialog::SendRequest(HWND hwnd) {
    Controls* c = GetControls(hwnd);
    std::wstring methodText = kMethods[SendMessageW(c->method, CB_GETCURSEL, 0, 0)];
    std::wstring urlText    = GetText(c->url);
    std::wstring headerText = BuildHeaderBlock(GetText(c->headers));
    std::wstring bodyText   = GetText(c->body);

    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256]{}, path[2048]{};
    uc.lpszHostName = host; uc.dwHostNameLength = 255;
    uc.lpszUrlPath  = path; uc.dwUrlPathLength  = 2047;
    if (!WinHttpCrackUrl(urlText.c_str(), (DWORD)urlText.size(), 0, &uc)) {
        SetWindowTextW(c->response, L"Could not parse URL. Include the scheme, e.g. https://example.com/path");
        return;
    }
    bool https = (uc.nScheme == INTERNET_SCHEME_HTTPS);

    HINTERNET hSession = WinHttpOpen(L"Blackhole/1.0 (Windows port)",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) { SetWindowTextW(c->response, L"WinHttpOpen failed."); return; }

    HINTERNET hConnect = WinHttpConnect(hSession, host, uc.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); SetWindowTextW(c->response, L"WinHttpConnect failed."); return; }

    DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, methodText.c_str(),
        path[0] ? path : L"/", nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        SetWindowTextW(c->response, L"WinHttpOpenRequest failed."); return;
    }

    bool hasBody = (methodText != L"GET" && methodText != L"DELETE") && !bodyText.empty();
    std::string bodyUtf8;
    if (hasBody) {
        int len = WideCharToMultiByte(CP_UTF8, 0, bodyText.c_str(), -1, nullptr, 0, nullptr, nullptr);
        bodyUtf8.resize(len > 0 ? len - 1 : 0);
        if (len > 0) WideCharToMultiByte(CP_UTF8, 0, bodyText.c_str(), -1, &bodyUtf8[0], len, nullptr, nullptr);
    }

    BOOL ok = WinHttpSendRequest(hRequest,
        headerText.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headerText.c_str(),
        headerText.empty() ? 0 : (DWORD)-1,
        hasBody ? (LPVOID)bodyUtf8.data() : WINHTTP_NO_REQUEST_DATA,
        hasBody ? (DWORD)bodyUtf8.size() : 0,
        hasBody ? (DWORD)bodyUtf8.size() : 0, 0);

    std::wstringstream result;
    if (ok && WinHttpReceiveResponse(hRequest, nullptr)) {
        DWORD statusCode = 0, size = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_FLAG_NUMBER | WINHTTP_QUERY_STATUS_CODE,
            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &size, WINHTTP_NO_HEADER_INDEX);

        DWORD headerLen = 0;
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX,
            nullptr, &headerLen, WINHTTP_NO_HEADER_INDEX);
        std::wstring rawHeaders(headerLen / sizeof(wchar_t), L'\0');
        if (headerLen > 0) {
            WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX,
                &rawHeaders[0], &headerLen, WINHTTP_NO_HEADER_INDEX);
        }

        result << L"Status: " << statusCode << L"\r\n" << rawHeaders << L"\r\n--- body ---\r\n";

        DWORD avail = 0;
        std::string bodyOut;
        do {
            avail = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &avail)) break;
            if (avail == 0) break;
            std::vector<char> buf(avail);
            DWORD read = 0;
            if (WinHttpReadData(hRequest, buf.data(), avail, &read)) {
                bodyOut.append(buf.data(), read);
            }
        } while (avail > 0);

        int wlen = MultiByteToWideChar(CP_UTF8, 0, bodyOut.c_str(), (int)bodyOut.size(), nullptr, 0);
        std::wstring wbody(wlen, L'\0');
        if (wlen > 0) MultiByteToWideChar(CP_UTF8, 0, bodyOut.c_str(), (int)bodyOut.size(), &wbody[0], wlen);
        result << wbody;
    } else {
        result << L"Request failed. Windows error code: " << GetLastError();
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    SetWindowTextW(c->response, result.str().c_str());
}

LRESULT CALLBACK PostmanDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            Controls* c = new Controls();
            c->method = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                10, 10, 100, 200, hwnd, (HMENU)(INT_PTR)ID_METHOD, GetModuleHandleW(nullptr), nullptr);
            for (auto m : kMethods) SendMessageW(c->method, CB_ADDSTRING, 0, (LPARAM)m);
            SendMessageW(c->method, CB_SETCURSEL, 0, 0);

            c->url = CreateWindowW(L"EDIT", L"https://", WS_CHILD | WS_VISIBLE | WS_BORDER,
                120, 10, 480, 24, hwnd, (HMENU)(INT_PTR)ID_URL, GetModuleHandleW(nullptr), nullptr);

            CreateWindowW(L"STATIC", L"Headers (one 'Name: value' per line):", WS_CHILD | WS_VISIBLE,
                10, 44, 300, 18, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            c->headers = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | WS_VSCROLL,
                10, 64, 600, 70, hwnd, (HMENU)(INT_PTR)ID_HEADERS, GetModuleHandleW(nullptr), nullptr);

            CreateWindowW(L"STATIC", L"Body:", WS_CHILD | WS_VISIBLE,
                10, 140, 300, 18, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            c->body = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | WS_VSCROLL,
                10, 160, 600, 80, hwnd, (HMENU)(INT_PTR)ID_BODY, GetModuleHandleW(nullptr), nullptr);

            c->send = CreateWindowW(L"BUTTON", L"Send", WS_CHILD | WS_VISIBLE,
                10, 248, 100, 28, hwnd, (HMENU)(INT_PTR)ID_SEND, GetModuleHandleW(nullptr), nullptr);

            c->response = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_HSCROLL,
                10, 284, 600, 230, hwnd, (HMENU)(INT_PTR)ID_RESPONSE, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HWND kids[] = { c->method, c->url, c->headers, c->body, c->send, c->response };
            for (auto k : kids) SendMessageW(k, WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)c);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_SEND && HIWORD(wParam) == BN_CLICKED) {
                SendRequest(hwnd);
            }
            return 0;
        case WM_DESTROY: {
            delete GetControls(hwnd);
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
