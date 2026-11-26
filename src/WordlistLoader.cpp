#include "WordlistLoader.h"
#include <windows.h>
#include <shlwapi.h>
#include <fstream>
#include <sstream>

#pragma comment(lib, "shlwapi.lib")

std::wstring ExeDirectory() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath);
    return exePath;
}

std::vector<std::wstring> LoadWordlistFile(const std::wstring& path) {
    std::vector<std::wstring> out;
    std::wifstream f(path);
    if (!f.is_open()) return out;
    std::wstring line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        size_t a = line.find_first_not_of(L" \t");
        if (a == std::wstring::npos) continue;
        size_t b = line.find_last_not_of(L" \t");
        out.push_back(line.substr(a, b - a + 1));
    }
    return out;
}

std::vector<std::wstring> ParseWordlistText(const std::wstring& text) {
    std::vector<std::wstring> out;
    std::wstringstream ss(text);
    std::wstring line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        size_t a = line.find_first_not_of(L" \t");
        if (a == std::wstring::npos) continue;
        size_t b = line.find_last_not_of(L" \t");
        out.push_back(line.substr(a, b - a + 1));
    }
    return out;
}
