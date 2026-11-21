#include "DataToolkitDialog.h"
#include <wincrypt.h>
#include <bcrypt.h>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "bcrypt.lib")

namespace {
    constexpr int ID_INPUT   = 301;
    constexpr int ID_OUTPUT  = 302;
    constexpr int ID_B64ENC  = 303;
    constexpr int ID_B64DEC  = 304;
    constexpr int ID_MD5     = 305;
    constexpr int ID_SHA1    = 306;
    constexpr int ID_SHA256  = 307;
    const wchar_t* kClassName = L"BlackholeDataToolkit";

    struct Controls { HWND input, output; };
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

    std::wstring FromUtf8(const std::string& s) {
        if (s.empty()) return {};
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
        std::wstring w(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], len);
        return w;
    }

    std::wstring Base64Encode(const std::string& data) {
        DWORD outLen = 0;
        CryptBinaryToStringA((const BYTE*)data.data(), (DWORD)data.size(),
            CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &outLen);
        std::string out(outLen, '\0');
        CryptBinaryToStringA((const BYTE*)data.data(), (DWORD)data.size(),
            CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &out[0], &outLen);
        if (!out.empty() && out.back() == '\0') out.pop_back();
        return FromUtf8(out);
    }

    std::wstring Base64Decode(const std::wstring& text) {
        std::string in = ToUtf8(text);
        DWORD outLen = 0;
        if (!CryptStringToBinaryA(in.c_str(), (DWORD)in.size(), CRYPT_STRING_BASE64,
                                   nullptr, &outLen, nullptr, nullptr)) {
            return L"(invalid base64)";
        }
        std::vector<BYTE> out(outLen);
        CryptStringToBinaryA(in.c_str(), (DWORD)in.size(), CRYPT_STRING_BASE64,
                              out.data(), &outLen, nullptr, nullptr);
        // Show as UTF-8 text if printable, otherwise as hex.
        return FromUtf8(std::string((char*)out.data(), outLen));
    }

    std::wstring HashHex(const std::string& data, LPCWSTR algId) {
        BCRYPT_ALG_HANDLE hAlg = nullptr;
        if (BCryptOpenAlgorithmProvider(&hAlg, algId, nullptr, 0) != 0) return L"(hash provider unavailable)";

        DWORD hashLen = 0, cbResult = 0;
        BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashLen, sizeof(hashLen), &cbResult, 0);
        std::vector<UCHAR> hash(hashLen);

        BCRYPT_HASH_HANDLE hHash = nullptr;
        BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
        BCryptHashData(hHash, (PUCHAR)data.data(), (ULONG)data.size(), 0);
        BCryptFinishHash(hHash, hash.data(), hashLen, 0);
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);

        std::wstringstream hex;
        for (UCHAR b : hash) hex << std::hex << std::setw(2) << std::setfill(L'0') << (int)b;
        return hex.str();
    }
}

void DataToolkitDialog::RegisterClassOnce() {
    static bool registered = false;
    if (registered) return;
    WNDCLASSW wc{};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = kClassName;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);
    registered = true;
}

void DataToolkitDialog::ShowFor(HWND owner) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - Data Toolkit",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 520, 420,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(hwnd, SW_SHOW);
}

void DataToolkitDialog::RunTransform(HWND hwnd, int action) {
    Controls* c = GetControls(hwnd);
    std::wstring in = GetText(c->input);
    std::wstring out;
    switch (action) {
        case ID_B64ENC: out = Base64Encode(ToUtf8(in)); break;
        case ID_B64DEC: out = Base64Decode(in); break;
        case ID_MD5:    out = HashHex(ToUtf8(in), BCRYPT_MD5_ALGORITHM); break;
        case ID_SHA1:   out = HashHex(ToUtf8(in), BCRYPT_SHA1_ALGORITHM); break;
        case ID_SHA256: out = HashHex(ToUtf8(in), BCRYPT_SHA256_ALGORITHM); break;
    }
    SetWindowTextW(c->output, out.c_str());
}

LRESULT CALLBACK DataToolkitDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            Controls* c = new Controls();
            CreateWindowW(L"STATIC", L"Input:", WS_CHILD | WS_VISIBLE, 10, 10, 100, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            c->input = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | WS_VSCROLL,
                10, 30, 480, 120, hwnd, (HMENU)(INT_PTR)ID_INPUT, GetModuleHandleW(nullptr), nullptr);

            struct { int id; const wchar_t* label; int x; } buttons[] = {
                { ID_B64ENC, L"Base64 Encode", 10 },
                { ID_B64DEC, L"Base64 Decode", 140 },
                { ID_MD5,    L"MD5",           270 },
                { ID_SHA1,   L"SHA-1",         340 },
                { ID_SHA256, L"SHA-256",       410 },
            };
            for (auto& b : buttons) {
                CreateWindowW(L"BUTTON", b.label, WS_CHILD | WS_VISIBLE,
                    b.x, 160, 120, 26, hwnd, (HMENU)(INT_PTR)b.id, GetModuleHandleW(nullptr), nullptr);
            }

            CreateWindowW(L"STATIC", L"Output:", WS_CHILD | WS_VISIBLE, 10, 196, 100, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            c->output = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL, 10, 216, 480, 150,
                hwnd, (HMENU)(INT_PTR)ID_OUTPUT, GetModuleHandleW(nullptr), nullptr);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)c);
            return 0;
        }
        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED) RunTransform(hwnd, LOWORD(wParam));
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
