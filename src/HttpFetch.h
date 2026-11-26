#pragma once
#include <string>
#include <vector>

// Shared WinHTTP GET helper for Stage 4's recon tools (SecurityHeaderScanner,
// TechFingerprintDialog, RobotsFetchDialog, FaviconHashDialog all just need
// "fetch this URL, give me headers + raw bytes") so the WinHTTP boilerplate
// PostmanDialog.cpp has isn't duplicated four more times.
struct HttpFetchResult {
    bool ok = false;
    DWORD statusCode = 0;
    std::wstring rawHeaders;      // CRLF-joined, as the server sent them
    std::vector<unsigned char> body; // raw bytes - callers decide text vs binary
    std::wstring error;           // set when ok == false
};

HttpFetchResult HttpFetchGet(const std::wstring& url, bool headOnly = false);

// Stage 5: added for GraphQLIntrospectionDialog, which needs to POST a
// JSON query body - everything else so far only needed GET/HEAD.
HttpFetchResult HttpFetchPost(const std::wstring& url, const std::wstring& bodyUtf16,
                               const std::wstring& contentType = L"application/json");

// Convenience: pulls one header's value out of rawHeaders (case-insensitive
// name match). Returns empty if absent - callers use that to mean "missing".
std::wstring HttpFindHeader(const std::wstring& rawHeaders, const std::wstring& name);
