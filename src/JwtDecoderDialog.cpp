#include "JwtDecoderDialog.h"
#include <wincrypt.h>
#include <string>
#include <vector>

#pragma comment(lib, "crypt32.lib")

namespace {
    constexpr int ID_INPUT = 601, ID_DECODE = 602, ID_HEADER = 603, ID_PAYLOAD = 604, ID_WARN = 605;
    const wchar_t* kClassName = L"BlackholeJwtDecoder";

    struct Controls { HWND input, header, payload; };
    Controls* GetControls(HWND h) { return reinterpret_cast<Controls*>(GetWindowLongPtr(h, GWLP_USERDATA)); }

    std::wstring GetText(HWND h) {
        int len = GetWindowTextLengthW(h);
        std::wstring s(len, L'\0');
        if (len > 0) GetWindowTextW(h, &s[0], len + 1);
        return s;
    }

    std::wstring FromUtf8(const std::string& s) {
        if (s.empty()) return {};
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
        std::wstring w(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], len);
        return w;
    }

    // Base64URL -> Base64 -> bytes -> UTF-8 text.
    std::wstring DecodeBase64UrlSegment(std::wstring seg) {
        for (auto& c : seg) { if (c == L'-') c = L'+'; else if (c == L'_') c = L'/'; }
        while (seg.size() % 4 != 0) seg += L'=';

        std::string in(seg.begin(), seg.end()); // JWT segments are always ASCII
        DWORD outLen = 0;
        if (!CryptStringToBinaryA(in.c_str(), (DWORD)in.size(), CRYPT_STRING_BASE64,
                                   nullptr, &outLen, nullptr, nullptr)) {
            return L"(could not base64url-decode this segment)";
        }
        std::vector<BYTE> out(outLen);
        CryptStringToBinaryA(in.c_str(), (DWORD)in.size(), CRYPT_STRING_BASE64,
                              out.data(), &outLen, nullptr, nullptr);
        return FromUtf8(std::string((char*)out.data(), outLen));
    }
}

void JwtDecoderDialog::RegisterClassOnce() {
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

void JwtDecoderDialog::ShowFor(HWND owner) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - JWT Decoder (decode only, no signature check)",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 560, 460,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(hwnd, SW_SHOW);
}

void JwtDecoderDialog::Decode(HWND hwnd) {
    Controls* c = GetControls(hwnd);
    std::wstring jwt = GetText(c->input);

    size_t dot1 = jwt.find(L'.');
    size_t dot2 = (dot1 == std::wstring::npos) ? std::wstring::npos : jwt.find(L'.', dot1 + 1);
    if (dot1 == std::wstring::npos || dot2 == std::wstring::npos) {
        SetWindowTextW(c->header, L"Not a JWT - expected three dot-separated segments (header.payload.signature).");
        SetWindowTextW(c->payload, L"");
        return;
    }

    std::wstring headerSeg  = jwt.substr(0, dot1);
    std::wstring payloadSeg = jwt.substr(dot1 + 1, dot2 - dot1 - 1);

    SetWindowTextW(c->header, DecodeBase64UrlSegment(headerSeg).c_str());
    SetWindowTextW(c->payload, DecodeBase64UrlSegment(payloadSeg).c_str());
}

LRESULT CALLBACK JwtDecoderDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            Controls* c = new Controls();
            CreateWindowW(L"STATIC", L"JWT:", WS_CHILD | WS_VISIBLE, 10, 10, 100, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            c->input = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | WS_VSCROLL,
                10, 30, 520, 60, hwnd, (HMENU)(INT_PTR)ID_INPUT, GetModuleHandleW(nullptr), nullptr);

            CreateWindowW(L"BUTTON", L"Decode", WS_CHILD | WS_VISIBLE, 10, 96, 100, 26,
                hwnd, (HMENU)(INT_PTR)ID_DECODE, GetModuleHandleW(nullptr), nullptr);
            CreateWindowW(L"STATIC", L"Decoded only - signature NOT verified.", WS_CHILD | WS_VISIBLE,
                120, 100, 300, 18, hwnd, (HMENU)(INT_PTR)ID_WARN, GetModuleHandleW(nullptr), nullptr);

            CreateWindowW(L"STATIC", L"Header:", WS_CHILD | WS_VISIBLE, 10, 132, 200, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            c->header = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL, 10, 152, 520, 100,
                hwnd, (HMENU)(INT_PTR)ID_HEADER, GetModuleHandleW(nullptr), nullptr);

            CreateWindowW(L"STATIC", L"Payload:", WS_CHILD | WS_VISIBLE, 10, 260, 200, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            c->payload = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL, 10, 280, 520, 140,
                hwnd, (HMENU)(INT_PTR)ID_PAYLOAD, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HWND kids[] = { c->input, c->header, c->payload };
            for (auto k : kids) SendMessageW(k, WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)c);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_DECODE && HIWORD(wParam) == BN_CLICKED) Decode(hwnd);
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
