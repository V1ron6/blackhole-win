#include "HarExportDialog.h"
#include <commdlg.h>
#include <fstream>
#include <sstream>
#include <vector>

#pragma comment(lib, "comdlg32.lib")

namespace {
    constexpr int ID_PREVIEW = 2501, ID_EXPORT = 2502;
    const wchar_t* kClassName = L"BlackholeHarExport";

    struct State {
        HWND preview;
        HarExportDialog::GetLogFn getLog;
    };
    State* GetState(HWND h) { return reinterpret_cast<State*>(GetWindowLongPtr(h, GWLP_USERDATA)); }

    // Splits "HH:MM:SS.mmm BLOCKED  <url>" / "HH:MM:SS.mmm allowed  <url>"
    // lines (RequestLogEntry.kt's format, see BlackholeApp::HookAdBlocking)
    // into (blocked, url) pairs - the timestamp itself isn't needed here
    // (RequestTimelineDialog is what uses it), just BLOCKED/allowed + URL.
    std::vector<std::pair<bool, std::wstring>> ParseLog(const std::wstring& log) {
        std::vector<std::pair<bool, std::wstring>> out;
        std::wstringstream ss(log);
        std::wstring line;
        while (std::getline(ss, line)) {
            if (!line.empty() && line.back() == L'\r') line.pop_back();
            if (line.empty()) continue;
            size_t firstSpace = line.find(L' ');
            if (firstSpace == std::wstring::npos) continue;
            std::wstring rest = line.substr(firstSpace + 1); // "BLOCKED  url" or "allowed  url"
            bool blocked = rest.rfind(L"BLOCKED", 0) == 0;
            size_t sp = rest.find(L"  ");
            if (sp == std::wstring::npos) continue;
            out.push_back({ blocked, rest.substr(sp + 2) });
        }
        return out;
    }

    std::string ToUtf8(const std::wstring& w) {
        if (w.empty()) return {};
        int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
        std::string s(len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], len, nullptr, nullptr);
        return s;
    }

    // Escapes a string for embedding in JSON (quotes/backslashes only -
    // URLs in this log don't contain control characters in practice).
    std::string JsonEscape(const std::string& s) {
        std::string out;
        for (char c : s) {
            if (c == '"' || c == '\\') out += '\\';
            out += c;
        }
        return out;
    }

    std::string BuildHar(const std::vector<std::pair<bool, std::wstring>>& entries) {
        std::stringstream out;
        out << "{\n  \"log\": {\n    \"version\": \"1.2\",\n"
            << "    \"creator\": { \"name\": \"Blackhole\", \"version\": \"1.0\" },\n"
            << "    \"entries\": [\n";
        for (size_t i = 0; i < entries.size(); ++i) {
            out << "      {\n"
                << "        \"startedDateTime\": \"1970-01-01T00:00:00.000Z\",\n" // see header comment: no per-request timestamp captured yet
                << "        \"time\": 0,\n"
                << "        \"request\": { \"method\": \"GET\", \"url\": \"" << JsonEscape(ToUtf8(entries[i].second)) << "\", \"headers\": [], \"queryString\": [] },\n"
                << "        \"response\": { \"status\": " << (entries[i].first ? 403 : 0) << ", \"statusText\": \""
                << (entries[i].first ? "Blocked by Blackhole ad blocker" : "") << "\", \"headers\": [], \"content\": { \"size\": 0, \"mimeType\": \"\" } },\n"
                << "        \"cache\": {}, \"timings\": { \"send\": 0, \"wait\": 0, \"receive\": 0 }\n"
                << "      }" << (i + 1 < entries.size() ? "," : "") << "\n";
        }
        out << "    ]\n  }\n}\n";
        return out.str();
    }
}

void HarExportDialog::RegisterClassOnce() {
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

void HarExportDialog::ShowFor(HWND owner, GetLogFn getLog) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - Export Request Log as .har",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 560, 460,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    State* st = GetState(hwnd);
    if (st) {
        st->getLog = std::move(getLog);
        if (st->getLog) SetWindowTextW(st->preview, st->getLog().c_str());
    }
    ShowWindow(hwnd, SW_SHOW);
}

void HarExportDialog::Export(HWND hwnd) {
    State* st = GetState(hwnd);
    if (!st->getLog) return;
    auto entries = ParseLog(st->getLog());
    if (entries.empty()) {
        MessageBoxW(hwnd, L"The active tab's request log is empty - nothing to export.",
            L"Blackhole", MB_ICONINFORMATION);
        return;
    }

    wchar_t fileBuf[MAX_PATH] = L"blackhole-export.har";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"HAR files (*.har)\0*.har\0All files\0*.*\0";
    ofn.lpstrDefExt = L"har";
    ofn.Flags = OFN_OVERWRITEPROMPT;
    if (!GetSaveFileNameW(&ofn)) return;

    std::string har = BuildHar(entries);
    std::ofstream f(fileBuf, std::ios::binary);
    if (!f.is_open()) {
        MessageBoxW(hwnd, L"Could not write the file.", L"Blackhole", MB_ICONERROR);
        return;
    }
    f.write(har.data(), har.size());
    f.close();
    MessageBoxW(hwnd, (std::wstring(L"Exported ") + std::to_wstring(entries.size()) +
        L" entries to:\n" + fileBuf).c_str(), L"Blackhole", MB_ICONINFORMATION);
}

LRESULT CALLBACK HarExportDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            State* st = new State();
            CreateWindowW(L"STATIC",
                L"Preview of the active tab's request log (see docs for what fields the .har export omits):",
                WS_CHILD | WS_VISIBLE, 10, 10, 520, 18, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            st->preview = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL, 10, 32, 520, 350,
                hwnd, (HMENU)(INT_PTR)ID_PREVIEW, GetModuleHandleW(nullptr), nullptr);
            CreateWindowW(L"BUTTON", L"Export as .har...", WS_CHILD | WS_VISIBLE, 10, 392, 150, 28,
                hwnd, (HMENU)(INT_PTR)ID_EXPORT, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            SendMessageW(st->preview, WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)st);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_EXPORT && HIWORD(wParam) == BN_CLICKED) Export(hwnd);
            return 0;
        case WM_DESTROY:
            delete GetState(hwnd);
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
