#include "TlsInspectorDialog.h"
#include <winhttp.h>
#include <string>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

namespace {
    constexpr int ID_URL = 1301, ID_INSPECT = 1302, ID_RESULTS = 1303;
    const wchar_t* kClassName = L"BlackholeTlsInspector";

    struct Controls { HWND url, results; };
    Controls* GetControls(HWND h) { return reinterpret_cast<Controls*>(GetWindowLongPtr(h, GWLP_USERDATA)); }

    std::wstring GetText(HWND h) {
        int len = GetWindowTextLengthW(h);
        std::wstring s(len, L'\0');
        if (len > 0) GetWindowTextW(h, &s[0], len + 1);
        return s;
    }

    std::wstring FileTimeToString(const FILETIME& ft) {
        SYSTEMTIME st{};
        FileTimeToSystemTime(&ft, &st);
        wchar_t buf[64];
        wsprintfW(buf, L"%04d-%02d-%02d %02d:%02d UTC", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
        return buf;
    }
}

void TlsInspectorDialog::RegisterClassOnce() {
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

void TlsInspectorDialog::ShowFor(HWND owner) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - TLS Inspector",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 560, 440,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(hwnd, SW_SHOW);
}

void TlsInspectorDialog::Inspect(HWND hwnd) {
    Controls* c = GetControls(hwnd);
    std::wstring url = GetText(c->url);
    std::wstringstream out;

    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256]{}, path[2048]{};
    uc.lpszHostName = host; uc.dwHostNameLength = 255;
    uc.lpszUrlPath  = path; uc.dwUrlPathLength  = 2047;
    if (!WinHttpCrackUrl(url.c_str(), (DWORD)url.size(), 0, &uc) || uc.nScheme != INTERNET_SCHEME_HTTPS) {
        SetWindowTextW(c->results, L"Enter an https:// URL - TLS inspection needs a secure connection to inspect.");
        return;
    }

    HINTERNET hSession = WinHttpOpen(L"Blackhole/1.0 (Windows port)",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET hConnect = hSession ? WinHttpConnect(hSession, host, uc.nPort, 0) : nullptr;
    HINTERNET hRequest = hConnect ? WinHttpOpenRequest(hConnect, L"GET", path[0] ? path : L"/",
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;

    bool ok = hRequest && WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) ok = WinHttpReceiveResponse(hRequest, nullptr);

    if (ok) {
        WINHTTP_CERTIFICATE_INFO certInfo{};
        DWORD size = sizeof(certInfo);
        if (WinHttpQueryOption(hRequest, WINHTTP_OPTION_SECURITY_CERTIFICATE_STRUCT, &certInfo, &size)) {
            out << L"Protocol:    " << (certInfo.lpszProtocolName ? certInfo.lpszProtocolName : L"(unknown)") << L"\r\n";
            out << L"Cipher:      " << (certInfo.lpszEncryptionAlgName ? certInfo.lpszEncryptionAlgName : L"(unknown)") << L"\r\n";
            out << L"Signature:   " << (certInfo.lpszSignatureAlgName ? certInfo.lpszSignatureAlgName : L"(unknown)") << L"\r\n";
            out << L"Key size:    " << certInfo.dwKeySize << L" bits\r\n\r\n";
            out << L"Subject:     " << (certInfo.lpszSubjectInfo ? certInfo.lpszSubjectInfo : L"(unknown)") << L"\r\n";
            out << L"Issuer:      " << (certInfo.lpszIssuerInfo ? certInfo.lpszIssuerInfo : L"(unknown)") << L"\r\n";
            out << L"Valid from:  " << FileTimeToString(certInfo.ftStart) << L"\r\n";
            out << L"Valid until: " << FileTimeToString(certInfo.ftExpiry) << L"\r\n";

            // These four strings are allocated by WinHTTP for this call -
            // must be freed with LocalFree, not delete/CoTaskMemFree.
            if (certInfo.lpszSubjectInfo) LocalFree(certInfo.lpszSubjectInfo);
            if (certInfo.lpszIssuerInfo) LocalFree(certInfo.lpszIssuerInfo);
            if (certInfo.lpszProtocolName) LocalFree(certInfo.lpszProtocolName);
            if (certInfo.lpszSignatureAlgName) LocalFree(certInfo.lpszSignatureAlgName);
            if (certInfo.lpszEncryptionAlgName) LocalFree(certInfo.lpszEncryptionAlgName);
        } else {
            out << L"Connected, but could not read certificate details (error " << GetLastError() << L").";
        }
    } else {
        out << L"Could not connect. Windows error code: " << GetLastError();
    }

    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);
    SetWindowTextW(c->results, out.str().c_str());
}

LRESULT CALLBACK TlsInspectorDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            Controls* c = new Controls();
            CreateWindowW(L"STATIC", L"https:// URL:", WS_CHILD | WS_VISIBLE, 10, 10, 150, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            c->url = CreateWindowW(L"EDIT", L"https://", WS_CHILD | WS_VISIBLE | WS_BORDER,
                10, 30, 400, 24, hwnd, (HMENU)(INT_PTR)ID_URL, GetModuleHandleW(nullptr), nullptr);
            CreateWindowW(L"BUTTON", L"Inspect", WS_CHILD | WS_VISIBLE, 420, 30, 110, 24,
                hwnd, (HMENU)(INT_PTR)ID_INSPECT, GetModuleHandleW(nullptr), nullptr);
            c->results = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL, 10, 64, 520, 330,
                hwnd, (HMENU)(INT_PTR)ID_RESULTS, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HWND kids[] = { c->url, c->results };
            for (auto k : kids) SendMessageW(k, WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)c);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_INSPECT && HIWORD(wParam) == BN_CLICKED) Inspect(hwnd);
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
