#include "TotpGeneratorDialog.h"
#include <bcrypt.h>
#include <string>
#include <vector>
#include <cwctype>
#include <ctime>

#pragma comment(lib, "bcrypt.lib")

namespace {
    constexpr int ID_SECRET = 901, ID_CODE = 902, ID_TIMELEFT = 903;
    constexpr UINT_PTR TIMER_ID = 1;
    constexpr int kPeriodSeconds = 30;
    constexpr int kDigits = 6;
    const wchar_t* kClassName = L"BlackholeTotpGenerator";

    struct Controls { HWND secret, code, timeLeft; };
    Controls* GetControls(HWND h) { return reinterpret_cast<Controls*>(GetWindowLongPtr(h, GWLP_USERDATA)); }

    std::wstring GetText(HWND h) {
        int len = GetWindowTextLengthW(h);
        std::wstring s(len, L'\0');
        if (len > 0) GetWindowTextW(h, &s[0], len + 1);
        return s;
    }

    // RFC 4648 Base32 decode. Ignores spaces/padding/casing, same
    // tolerant parsing most authenticator secret fields use.
    std::vector<BYTE> Base32Decode(const std::wstring& in) {
        static const wchar_t* kAlphabet = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
        std::vector<BYTE> out;
        uint64_t buffer = 0;
        int bitsLeft = 0;
        for (wchar_t raw : in) {
            wchar_t c = towupper(raw);
            if (c == L' ' || c == L'-' || c == L'=') continue;
            const wchar_t* pos = wcschr(kAlphabet, c);
            if (!pos) continue; // skip anything outside the alphabet
            int val = (int)(pos - kAlphabet);
            buffer = (buffer << 5) | (uint32_t)val;
            bitsLeft += 5;
            if (bitsLeft >= 8) {
                bitsLeft -= 8;
                out.push_back((BYTE)((buffer >> bitsLeft) & 0xFF));
            }
        }
        return out;
    }

    // RFC 6238 HOTP/TOTP over CNG's HMAC-SHA1.
    std::wstring ComputeTotp(const std::vector<BYTE>& key, long long counter) {
        if (key.empty()) return L"------";

        BYTE counterBytes[8];
        for (int i = 7; i >= 0; --i) { counterBytes[i] = (BYTE)(counter & 0xFF); counter >>= 8; }

        BCRYPT_ALG_HANDLE hAlg = nullptr;
        if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA1_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0)
            return L"(HMAC unavailable)";

        BCRYPT_HASH_HANDLE hHash = nullptr;
        BCryptCreateHash(hAlg, &hHash, nullptr, 0, (PUCHAR)key.data(), (ULONG)key.size(), 0);
        BCryptHashData(hHash, counterBytes, sizeof(counterBytes), 0);

        BYTE digest[20];
        BCryptFinishHash(hHash, digest, sizeof(digest), 0);
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);

        int offset = digest[19] & 0x0F;
        uint32_t binCode = ((digest[offset] & 0x7F) << 24) | ((digest[offset + 1] & 0xFF) << 16) |
                            ((digest[offset + 2] & 0xFF) << 8) | (digest[offset + 3] & 0xFF);

        uint32_t mod = 1;
        for (int i = 0; i < kDigits; ++i) mod *= 10;
        uint32_t code = binCode % mod;

        wchar_t buf[16];
        wsprintfW(buf, L"%0*u", kDigits, code);
        return buf;
    }
}

void TotpGeneratorDialog::RegisterClassOnce() {
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

void TotpGeneratorDialog::ShowFor(HWND owner) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - TOTP Generator",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 400, 220,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(hwnd, SW_SHOW);
}

void TotpGeneratorDialog::Tick(HWND hwnd) {
    Controls* c = GetControls(hwnd);
    std::wstring secretText = GetText(c->secret);
    auto key = Base32Decode(secretText);

    time_t now = time(nullptr);
    long long counter = (long long)now / kPeriodSeconds;
    int secondsLeft = kPeriodSeconds - (int)(now % kPeriodSeconds);

    SetWindowTextW(c->code, secretText.empty() ? L"------" : ComputeTotp(key, counter).c_str());
    wchar_t buf[64];
    wsprintfW(buf, L"Refreshes in %d s", secondsLeft);
    SetWindowTextW(c->timeLeft, buf);
}

LRESULT CALLBACK TotpGeneratorDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            Controls* c = new Controls();
            CreateWindowW(L"STATIC", L"Secret (Base32, e.g. from a QR code):", WS_CHILD | WS_VISIBLE,
                10, 10, 360, 18, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            c->secret = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
                10, 30, 360, 24, hwnd, (HMENU)(INT_PTR)ID_SECRET, GetModuleHandleW(nullptr), nullptr);

            HFONT bigFont = CreateFontW(36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH, L"Consolas");
            c->code = CreateWindowW(L"STATIC", L"------", WS_CHILD | WS_VISIBLE | SS_CENTER,
                10, 70, 360, 50, hwnd, (HMENU)(INT_PTR)ID_CODE, GetModuleHandleW(nullptr), nullptr);
            SendMessageW(c->code, WM_SETFONT, (WPARAM)bigFont, TRUE);

            c->timeLeft = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER,
                10, 124, 360, 20, hwnd, (HMENU)(INT_PTR)ID_TIMELEFT, GetModuleHandleW(nullptr), nullptr);
            SendMessageW(c->timeLeft, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
            SendMessageW(c->secret, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)c);
            SetTimer(hwnd, TIMER_ID, 1000, nullptr);
            Tick(hwnd);
            return 0;
        }
        case WM_TIMER:
            if (wParam == TIMER_ID) Tick(hwnd);
            return 0;
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_SECRET && HIWORD(wParam) == EN_CHANGE) Tick(hwnd);
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, TIMER_ID);
            delete GetControls(hwnd);
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
