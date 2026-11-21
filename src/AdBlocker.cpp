#include "AdBlocker.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cwctype>

static std::wstring ToLowerCopy(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return (wchar_t)towlower(c); });
    return s;
}

size_t AdBlocker::LoadFromFile(const std::wstring& path) {
    blocked_.clear();
    std::wifstream f(path);
    if (!f.is_open()) return 0;

    std::wstring line;
    while (std::getline(f, line)) {
        // Strip \r if the file has CRLF line endings and we're reading on
        // a stream that didn't normalize them.
        if (!line.empty() && line.back() == L'\r') line.pop_back();

        size_t start = line.find_first_not_of(L" \t");
        if (start == std::wstring::npos) continue;
        if (line[start] == L'#') continue; // comment

        // Same tolerant parsing the Kotlin AdBlocker used: accept either a
        // bare hostname per line, or a hosts-file-style "0.0.0.0 host" /
        // "127.0.0.1 host" line (so you can drop in StevenBlack's list
        // unmodified, same as the README's "Improving the ad blocker" note).
        std::wistringstream iss(line.substr(start));
        std::wstring first, second;
        iss >> first;
        if (first == L"0.0.0.0" || first == L"127.0.0.1") {
            if (iss >> second) blocked_.insert(ToLowerCopy(second));
        } else {
            blocked_.insert(ToLowerCopy(first));
        }
    }
    return blocked_.size();
}

bool AdBlocker::IsBlocked(const std::wstring& host) const {
    if (blocked_.empty()) return false;
    std::wstring h = ToLowerCopy(host);
    if (blocked_.count(h)) return true;

    // Match parent-domain blocking the same way the Android version did:
    // a block entry for "ads.example.com" also covers "sub.ads.example.com".
    size_t dot = h.find(L'.');
    while (dot != std::wstring::npos) {
        h = h.substr(dot + 1);
        if (blocked_.count(h)) return true;
        dot = h.find(L'.');
    }
    return false;
}
