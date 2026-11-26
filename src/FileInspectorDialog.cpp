#include "FileInspectorDialog.h"
#include <commdlg.h>
#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <cstring>
#include <algorithm>

#pragma comment(lib, "comdlg32.lib")

namespace {
    constexpr int ID_PATH = 2301, ID_BROWSE = 2302, ID_RESULTS = 2303;
    const wchar_t* kClassName = L"BlackholeFileInspector";

    struct Controls { HWND path, results; };
    Controls* GetControls(HWND h) { return reinterpret_cast<Controls*>(GetWindowLongPtr(h, GWLP_USERDATA)); }

    // Magic-number table: {offset, bytes, label}. Checked in order, first match wins.
    struct Signature { size_t offset; const char* bytes; size_t len; const wchar_t* label; };
    const Signature kSignatures[] = {
        { 0, "\x89PNG\r\n\x1a\n", 8, L"PNG image" },
        { 0, "\xFF\xD8\xFF", 3, L"JPEG image" },
        { 0, "GIF87a", 6, L"GIF image" },
        { 0, "GIF89a", 6, L"GIF image" },
        { 0, "%PDF-", 5, L"PDF document" },
        { 0, "PK\x03\x04", 4, L"ZIP archive (or .docx/.xlsx/.pptx/.jar/.apk)" },
        { 0, "\x7fELF", 4, L"ELF executable (Linux)" },
        { 0, "MZ", 2, L"PE executable (.exe/.dll)" },
        { 0, "\x1f\x8b", 2, L"GZIP archive" },
        { 0, "Rar!\x1a\x07", 6, L"RAR archive" },
        { 0, "BM", 2, L"BMP image" },
        { 0, "RIFF", 4, L"RIFF container (WAV/AVI/WebP)" },
        { 0, "ID3", 3, L"MP3 audio (ID3 tag)" },
        { 0, "\x00\x00\x00\x18ftyp", 8, L"MP4 video" },
    };

    std::wstring GetText(HWND h) {
        int len = GetWindowTextLengthW(h);
        std::wstring s(len, L'\0');
        if (len > 0) GetWindowTextW(h, &s[0], len + 1);
        return s;
    }

    std::wstring GuessType(const std::vector<unsigned char>& bytes) {
        for (auto& sig : kSignatures) {
            if (bytes.size() < sig.offset + sig.len) continue;
            if (memcmp(bytes.data() + sig.offset, sig.bytes, sig.len) == 0) return sig.label;
        }
        return L"(unrecognized - not in the built-in signature table)";
    }

    std::wstring HexDump(const std::vector<unsigned char>& bytes, size_t maxBytes) {
        std::wstringstream out;
        size_t n = (bytes.size() < maxBytes) ? bytes.size() : maxBytes;
        for (size_t row = 0; row < n; row += 16) {
            wchar_t offsetBuf[16];
            wsprintfW(offsetBuf, L"%08zX  ", row);
            out << offsetBuf;
            for (size_t col = 0; col < 16; ++col) {
                if (row + col < n) {
                    wchar_t b[4];
                    wsprintfW(b, L"%02X ", bytes[row + col]);
                    out << b;
                } else {
                    out << L"   ";
                }
            }
            out << L" ";
            for (size_t col = 0; col < 16 && row + col < n; ++col) {
                unsigned char c = bytes[row + col];
                out << (wchar_t)((c >= 32 && c < 127) ? c : L'.');
            }
            out << L"\r\n";
        }
        return out.str();
    }
}

void FileInspectorDialog::RegisterClassOnce() {
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

void FileInspectorDialog::ShowFor(HWND owner) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - File Inspector",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 620, 480,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(hwnd, SW_SHOW);
}

void FileInspectorDialog::PickAndInspect(HWND hwnd) {
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
    if (!f.is_open()) {
        SetWindowTextW(c->results, L"Could not open file.");
        return;
    }
    std::streamsize size = f.tellg();
    f.seekg(0);
    std::vector<unsigned char> bytes((size_t)std::min((std::streamsize)4096, size));
    f.read((char*)bytes.data(), bytes.size());

    std::wstringstream out;
    out << L"Size: " << size << L" bytes\r\n";
    out << L"Type guess: " << GuessType(bytes) << L"\r\n\r\n";
    out << L"First " << bytes.size() << L" bytes:\r\n" << HexDump(bytes, bytes.size());
    SetWindowTextW(c->results, out.str().c_str());
}

LRESULT CALLBACK FileInspectorDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            Controls* c = new Controls();
            c->path = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY,
                10, 10, 490, 24, hwnd, (HMENU)(INT_PTR)ID_PATH, GetModuleHandleW(nullptr), nullptr);
            CreateWindowW(L"BUTTON", L"Browse...", WS_CHILD | WS_VISIBLE, 510, 10, 90, 24,
                hwnd, (HMENU)(INT_PTR)ID_BROWSE, GetModuleHandleW(nullptr), nullptr);
            c->results = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_HSCROLL, 10, 44, 590, 400,
                hwnd, (HMENU)(INT_PTR)ID_RESULTS, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HFONT monoFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH, L"Consolas");
            SendMessageW(c->path, WM_SETFONT, (WPARAM)font, TRUE);
            SendMessageW(c->results, WM_SETFONT, (WPARAM)monoFont, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)c);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_BROWSE && HIWORD(wParam) == BN_CLICKED) PickAndInspect(hwnd);
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
