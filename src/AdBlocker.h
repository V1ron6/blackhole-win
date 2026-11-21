#pragma once
#include <string>
#include <unordered_set>

// Direct port of AdBlocker.kt: host-based request blocking.
// Loads assets\adblock_hosts.txt (one hostname per line, '#' comments
// and blank lines skipped - same format the Kotlin version used) and
// answers IsBlocked(host) in O(1). Hooked into WebView2's
// add_WebResourceRequested in BlackholeApp.cpp, same place the Android
// version hooked shouldInterceptRequest.
class AdBlocker {
public:
    // dir = folder containing adblock_hosts.txt (the exe's own folder,
    // see BlackholeApp.cpp - assets are copied next to the .exe at build time).
    size_t LoadFromFile(const std::wstring& path);
    bool   IsBlocked(const std::wstring& host) const;
    size_t Count() const { return blocked_.size(); }

private:
    std::unordered_set<std::wstring> blocked_;
};
