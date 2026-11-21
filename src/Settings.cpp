#include "Settings.h"
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <map>

#pragma comment(lib, "shell32.lib")

const wchar_t* BrowserModeName(BrowserMode m) {
    switch (m) {
        case BrowserMode::Basic:        return L"Basic";
        case BrowserMode::Intermediate: return L"Intermediate";
        case BrowserMode::Advance:      return L"Advance";
        case BrowserMode::ByteBandit:   return L"ByteBandit";
    }
    return L"Basic";
}

const wchar_t* SecurityModeName(SecurityMode m) {
    return m == SecurityMode::Strict ? L"Strict (HTTPS only)" : L"CTF (HTTP allowed)";
}

std::wstring AppSettings::SettingsFilePath() {
    PWSTR appData = nullptr;
    std::wstring path = L".\\blackhole-settings.ini"; // fallback
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData))) {
        std::wstring dir = std::wstring(appData) + L"\\Blackhole";
        CreateDirectoryW(dir.c_str(), nullptr);
        path = dir + L"\\settings.ini";
        CoTaskMemFree(appData);
    }
    return path;
}

static std::wstring Trim(const std::wstring& s) {
    size_t a = s.find_first_not_of(L" \t\r\n");
    size_t b = s.find_last_not_of(L" \t\r\n");
    if (a == std::wstring::npos) return L"";
    return s.substr(a, b - a + 1);
}

AppSettings AppSettings::Load() {
    AppSettings s;
    std::wifstream f(SettingsFilePath());
    if (!f.is_open()) return s; // defaults

    std::map<std::wstring, std::wstring> kv;
    std::wstring line;
    while (std::getline(f, line)) {
        auto eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;
        kv[Trim(line.substr(0, eq))] = Trim(line.substr(eq + 1));
    }

    auto geti = [&](const wchar_t* k, int def) { return kv.count(k) ? _wtoi(kv[k].c_str()) : def; };
    auto getb = [&](const wchar_t* k, bool def) { return kv.count(k) ? (kv[k] == L"1") : def; };
    auto gets = [&](const wchar_t* k, const std::wstring& def) { return kv.count(k) ? kv[k] : def; };

    s.browserMode  = static_cast<BrowserMode>(geti(L"browserMode", 0));
    s.securityMode = static_cast<SecurityMode>(geti(L"securityMode", 0));
    s.proxyEnabled = getb(L"proxyEnabled", false);
    s.proxyScheme  = gets(L"proxyScheme", L"socks5");
    s.proxyHost    = gets(L"proxyHost", L"127.0.0.1");
    s.proxyPort    = geti(L"proxyPort", 9050);
    s.downloadRetentionHours = geti(L"downloadRetentionHours", 24);
    s.notificationsEnabled   = getb(L"notificationsEnabled", true);
    return s;
}

bool AppSettings::Save() const {
    std::wofstream f(SettingsFilePath(), std::ios::trunc);
    if (!f.is_open()) return false;
    f << L"browserMode="  << static_cast<int>(browserMode)  << L"\n";
    f << L"securityMode=" << static_cast<int>(securityMode) << L"\n";
    f << L"proxyEnabled=" << (proxyEnabled ? 1 : 0) << L"\n";
    f << L"proxyScheme="  << proxyScheme << L"\n";
    f << L"proxyHost="    << proxyHost   << L"\n";
    f << L"proxyPort="    << proxyPort   << L"\n";
    f << L"downloadRetentionHours=" << downloadRetentionHours << L"\n";
    f << L"notificationsEnabled="  << (notificationsEnabled ? 1 : 0) << L"\n";
    return true;
}
