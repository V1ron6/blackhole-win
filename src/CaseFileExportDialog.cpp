#include "CaseFileExportDialog.h"
#include <commdlg.h>
#include <fstream>
#include <string>
#include <cwchar>

#pragma comment(lib, "comdlg32.lib")

namespace {
    constexpr int ID_NOTES = 3101, ID_EXPORT = 3102;
    const wchar_t* kClassName = L"BlackholeCaseFileExport";

    struct Controls { HWND notes; };
    Controls* GetControls(HWND h) { return reinterpret_cast<Controls*>(GetWindowLongPtr(h, GWLP_USERDATA)); }

    std::wstring GetText(HWND h) {
        int len = GetWindowTextLengthW(h);
        std::wstring s(len, L'\0');
        if (len > 0) GetWindowTextW(h, &s[0], len + 1);
        return s;
    }

    std::string ToUtf8(const std::wstring& w) {
        if (w.empty()) return {};
        int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
        std::string s(len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], len, nullptr, nullptr);
        return s;
    }
}

void CaseFileExportDialog::RegisterClassOnce() {
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

void CaseFileExportDialog::ShowFor(HWND owner) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - Case File Export",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 520, 440,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(hwnd, SW_SHOW);
}

void CaseFileExportDialog::Export(HWND hwnd) {
    Controls* c = GetControls(hwnd);
    std::wstring notes = GetText(c->notes);

    SYSTEMTIME st; GetLocalTime(&st);
    wchar_t defaultName[64];
    wsprintfW(defaultName, L"blackhole-case-%04d%02d%02d-%02d%02d%02d.txt",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    wchar_t fileBuf[MAX_PATH];
    wcscpy_s(fileBuf, defaultName);
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Text files (*.txt)\0*.txt\0All files\0*.*\0";
    ofn.lpstrDefExt = L"txt";
    ofn.Flags = OFN_OVERWRITEPROMPT;
    if (!GetSaveFileNameW(&ofn)) return;

    std::string utf8 = ToUtf8(notes);
    std::ofstream f(fileBuf, std::ios::binary);
    if (!f.is_open()) { MessageBoxW(hwnd, L"Could not write the file.", L"Blackhole", MB_ICONERROR); return; }
    f.write(utf8.data(), utf8.size());
    f.close();
    MessageBoxW(hwnd, (std::wstring(L"Exported to:\n") + fileBuf).c_str(), L"Blackhole", MB_ICONINFORMATION);
}

LRESULT CALLBACK CaseFileExportDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            Controls* c = new Controls();
            CreateWindowW(L"STATIC", L"Case notes:", WS_CHILD | WS_VISIBLE, 10, 10, 200, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            c->notes = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | WS_VSCROLL,
                10, 30, 480, 350, hwnd, (HMENU)(INT_PTR)ID_NOTES, GetModuleHandleW(nullptr), nullptr);
            CreateWindowW(L"BUTTON", L"Export as .txt...", WS_CHILD | WS_VISIBLE, 10, 390, 150, 28,
                hwnd, (HMENU)(INT_PTR)ID_EXPORT, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            SendMessageW(c->notes, WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)c);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_EXPORT && HIWORD(wParam) == BN_CLICKED) Export(hwnd);
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
