#include "StringsExtractorDialog.h"
#include <commdlg.h>
#include <fstream>
#include <sstream>
#include <vector>

#pragma comment(lib, "comdlg32.lib")

namespace {
    constexpr int ID_PATH = 2901, ID_BROWSE = 2902, ID_RESULTS = 2903;
    constexpr size_t kMinLength = 4; // same default as Unix `strings`
    const wchar_t* kClassName = L"BlackholeStringsExtractor";

    struct Controls { HWND path, results; };
    Controls* GetControls(HWND h) { return reinterpret_cast<Controls*>(GetWindowLongPtr(h, GWLP_USERDATA)); }

    std::wstring ExtractStrings(const std::vector<unsigned char>& bytes) {
        std::wstringstream out;
        std::string current;
        size_t count = 0;
        constexpr size_t kMaxResults = 2000; // cap so a huge binary doesn't freeze the UI with output

        auto flush = [&]() {
            if (current.size() >= kMinLength && count < kMaxResults) {
                out << std::wstring(current.begin(), current.end()) << L"\r\n";
                count++;
            }
            current.clear();
        };
        for (unsigned char b : bytes) {
            if (b >= 32 && b < 127) current += (char)b;
            else flush();
            if (count >= kMaxResults) break;
        }
        flush();
        if (count >= kMaxResults) out << L"\r\n(truncated at " << kMaxResults << L" results)";
        return out.str();
    }
}

void StringsExtractorDialog::RegisterClassOnce() {
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

void StringsExtractorDialog::ShowFor(HWND owner) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - Strings Extractor",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 520, 480,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(hwnd, SW_SHOW);
}

void StringsExtractorDialog::PickAndExtract(HWND hwnd) {
    Controls* c = GetControls(hwnd);
    wchar_t fileBuf[MAX_PATH] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"All files\0*.*\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return;
    SetWindowTextW(c->path, fileBuf);

    std::ifstream f(fileBuf, std::ios::binary | std::ios::ate);
    if (!f.is_open()) { SetWindowTextW(c->results, L"Could not open file."); return; }
    std::streamsize size = f.tellg();
    f.seekg(0);
    std::vector<unsigned char> bytes((size_t)size);
    f.read((char*)bytes.data(), size);

    SetWindowTextW(c->results, ExtractStrings(bytes).c_str());
}

LRESULT CALLBACK StringsExtractorDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            Controls* c = new Controls();
            c->path = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY,
                10, 10, 390, 24, hwnd, (HMENU)(INT_PTR)ID_PATH, GetModuleHandleW(nullptr), nullptr);
            CreateWindowW(L"BUTTON", L"Browse...", WS_CHILD | WS_VISIBLE, 410, 10, 90, 24,
                hwnd, (HMENU)(INT_PTR)ID_BROWSE, GetModuleHandleW(nullptr), nullptr);
            c->results = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_HSCROLL, 10, 44, 490, 400,
                hwnd, (HMENU)(INT_PTR)ID_RESULTS, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HWND kids[] = { c->path, c->results };
            for (auto k : kids) SendMessageW(k, WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)c);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_BROWSE && HIWORD(wParam) == BN_CLICKED) PickAndExtract(hwnd);
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
