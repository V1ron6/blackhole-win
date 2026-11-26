#include "HttpFetch.h"
#include <winhttp.h>
#include <algorithm>
#include <cwctype>

#pragma comment(lib, "winhttp.lib")

HttpFetchResult HttpFetchGet(const std::wstring& url, bool headOnly) {
    HttpFetchResult result;

    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256]{}, path[2048]{};
    uc.lpszHostName = host; uc.dwHostNameLength = 255;
    uc.lpszUrlPath  = path; uc.dwUrlPathLength  = 2047;
    if (!WinHttpCrackUrl(url.c_str(), (DWORD)url.size(), 0, &uc)) {
        result.error = L"Could not parse URL - include the scheme, e.g. https://example.com";
        return result;
    }
    bool https = (uc.nScheme == INTERNET_SCHEME_HTTPS);

    HINTERNET hSession = WinHttpOpen(L"Blackhole/1.0 (Windows port)",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) { result.error = L"WinHttpOpen failed."; return result; }

    HINTERNET hConnect = WinHttpConnect(hSession, host, uc.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); result.error = L"WinHttpConnect failed."; return result; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, headOnly ? L"HEAD" : L"GET",
        path[0] ? path : L"/", nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, https ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        result.error = L"WinHttpOpenRequest failed."; return result;
    }

    // Follow redirects automatically (default), 10s timeouts so a dead host
    // doesn't hang the UI thread indefinitely.
    WinHttpSetTimeouts(hRequest, 10000, 10000, 10000, 10000);

    BOOL ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) ok = WinHttpReceiveResponse(hRequest, nullptr);

    if (ok) {
        DWORD statusCode = 0, size = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_FLAG_NUMBER | WINHTTP_QUERY_STATUS_CODE,
            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &size, WINHTTP_NO_HEADER_INDEX);
        result.statusCode = statusCode;

        DWORD headerLen = 0;
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX,
            nullptr, &headerLen, WINHTTP_NO_HEADER_INDEX);
        if (headerLen > 0) {
            result.rawHeaders.resize(headerLen / sizeof(wchar_t));
            WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX,
                &result.rawHeaders[0], &headerLen, WINHTTP_NO_HEADER_INDEX);
        }

        if (!headOnly) {
            DWORD avail = 0;
            do {
                avail = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &avail) || avail == 0) break;
                size_t oldSize = result.body.size();
                result.body.resize(oldSize + avail);
                DWORD read = 0;
                WinHttpReadData(hRequest, result.body.data() + oldSize, avail, &read);
                result.body.resize(oldSize + read);
            } while (avail > 0);
        }
        result.ok = true;
    } else {
        result.error = L"Request failed. Windows error code: " + std::to_wstring(GetLastError());
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

HttpFetchResult HttpFetchPost(const std::wstring& url, const std::wstring& bodyUtf16, const std::wstring& contentType) {
    HttpFetchResult result;

    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256]{}, path[2048]{};
    uc.lpszHostName = host; uc.dwHostNameLength = 255;
    uc.lpszUrlPath  = path; uc.dwUrlPathLength  = 2047;
    if (!WinHttpCrackUrl(url.c_str(), (DWORD)url.size(), 0, &uc)) {
        result.error = L"Could not parse URL - include the scheme, e.g. https://example.com";
        return result;
    }
    bool https = (uc.nScheme == INTERNET_SCHEME_HTTPS);

    HINTERNET hSession = WinHttpOpen(L"Blackhole/1.0 (Windows port)",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET hConnect = hSession ? WinHttpConnect(hSession, host, uc.nPort, 0) : nullptr;
    HINTERNET hRequest = hConnect ? WinHttpOpenRequest(hConnect, L"POST", path[0] ? path : L"/",
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, https ? WINHTTP_FLAG_SECURE : 0) : nullptr;
    if (!hRequest) {
        if (hConnect) WinHttpCloseHandle(hConnect);
        if (hSession) WinHttpCloseHandle(hSession);
        result.error = L"Could not open request.";
        return result;
    }
    WinHttpSetTimeouts(hRequest, 10000, 10000, 10000, 10000);

    int bodyLen = WideCharToMultiByte(CP_UTF8, 0, bodyUtf16.c_str(), (int)bodyUtf16.size(), nullptr, 0, nullptr, nullptr);
    std::string bodyUtf8(bodyLen, '\0');
    if (bodyLen > 0) WideCharToMultiByte(CP_UTF8, 0, bodyUtf16.c_str(), (int)bodyUtf16.size(), &bodyUtf8[0], bodyLen, nullptr, nullptr);

    std::wstring headers = L"Content-Type: " + contentType;
    BOOL ok = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)-1,
        (LPVOID)bodyUtf8.data(), (DWORD)bodyUtf8.size(), (DWORD)bodyUtf8.size(), 0);
    if (ok) ok = WinHttpReceiveResponse(hRequest, nullptr);

    if (ok) {
        DWORD statusCode = 0, size = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_FLAG_NUMBER | WINHTTP_QUERY_STATUS_CODE,
            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &size, WINHTTP_NO_HEADER_INDEX);
        result.statusCode = statusCode;

        DWORD avail = 0;
        do {
            avail = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &avail) || avail == 0) break;
            size_t oldSize = result.body.size();
            result.body.resize(oldSize + avail);
            DWORD read = 0;
            WinHttpReadData(hRequest, result.body.data() + oldSize, avail, &read);
            result.body.resize(oldSize + read);
        } while (avail > 0);
        result.ok = true;
    } else {
        result.error = L"Request failed. Windows error code: " + std::to_wstring(GetLastError());
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

std::wstring HttpFindHeader(const std::wstring& rawHeaders, const std::wstring& name) {
    auto lower = [](std::wstring s) {
        std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return (wchar_t)towlower(c); });
        return s;
    };
    std::wstring needle = lower(name) + L":";
    std::wstring hay = lower(rawHeaders);

    size_t pos = 0;
    while (true) {
        pos = hay.find(needle, pos);
        if (pos == std::wstring::npos) return L"";
        // Make sure this is the start of a header line, not a substring
        // match in the middle of another header's value.
        if (pos == 0 || rawHeaders[pos - 1] == L'\n' || rawHeaders[pos - 1] == L'\r') {
            size_t valueStart = pos + needle.size();
            size_t lineEnd = rawHeaders.find(L"\r\n", valueStart);
            std::wstring value = rawHeaders.substr(valueStart,
                lineEnd == std::wstring::npos ? std::wstring::npos : lineEnd - valueStart);
            size_t a = value.find_first_not_of(L" \t");
            size_t b = value.find_last_not_of(L" \t");
            return (a == std::wstring::npos) ? L"" : value.substr(a, b - a + 1);
        }
        pos += needle.size();
    }
}
