#include "KnownHostsStore.h"
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <map>

#pragma comment(lib, "shell32.lib")

std::wstring KnownHostsStore::FilePath() {
    PWSTR appData = nullptr;
    std::wstring path = L".\\blackhole-known-hosts.txt";
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData))) {
        std::wstring dir = std::wstring(appData) + L"\\Blackhole";
        CreateDirectoryW(dir.c_str(), nullptr);
        path = dir + L"\\known_hosts.txt";
        CoTaskMemFree(appData);
    }
    return path;
}

std::wstring KnownHostsStore::Key(const std::wstring& host, int port) {
    return host + L":" + std::to_wstring(port);
}

std::wstring KnownHostsStore::GetFingerprint(const std::wstring& host, int port) const {
    std::wifstream f(FilePath());
    if (!f.is_open()) return L"";
    std::wstring wantKey = Key(host, port);
    std::wstring line;
    while (std::getline(f, line)) {
        size_t eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;
        if (line.substr(0, eq) == wantKey) return line.substr(eq + 1);
    }
    return L"";
}

void KnownHostsStore::SaveFingerprint(const std::wstring& host, int port, const std::wstring& fingerprint) {
    std::wstring wantKey = Key(host, port);
    std::map<std::wstring, std::wstring> all;
    {
        std::wifstream in(FilePath());
        std::wstring line;
        while (std::getline(in, line)) {
            size_t eq = line.find(L'=');
            if (eq == std::wstring::npos) continue;
            all[line.substr(0, eq)] = line.substr(eq + 1);
        }
    }
    all[wantKey] = fingerprint;
    std::wofstream out(FilePath(), std::ios::trunc);
    for (auto& kv : all) out << kv.first << L"=" << kv.second << L"\n";
}

void KnownHostsStore::Forget(const std::wstring& host, int port) {
    std::wstring wantKey = Key(host, port);
    std::map<std::wstring, std::wstring> all;
    {
        std::wifstream in(FilePath());
        std::wstring line;
        while (std::getline(in, line)) {
            size_t eq = line.find(L'=');
            if (eq == std::wstring::npos) continue;
            all[line.substr(0, eq)] = line.substr(eq + 1);
        }
    }
    all.erase(wantKey);
    std::wofstream out(FilePath(), std::ios::trunc);
    for (auto& kv : all) out << kv.first << L"=" << kv.second << L"\n";
}
