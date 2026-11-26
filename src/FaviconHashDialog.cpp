#include "FaviconHashDialog.h"
#include "HttpFetch.h"
#include <wincrypt.h>
#include <string>
#include <sstream>
#include <cstdint>
#include <cstring>

#pragma comment(lib, "crypt32.lib")

namespace {
    constexpr int ID_URL = 1701, ID_HASH = 1702, ID_RESULTS = 1703;
    const wchar_t* kClassName = L"BlackholeFaviconHash";

    struct Controls { HWND url, results; };
    Controls* GetControls(HWND h) { return reinterpret_cast<Controls*>(GetWindowLongPtr(h, GWLP_USERDATA)); }

    std::wstring GetText(HWND h) {
        int len = GetWindowTextLengthW(h);
        std::wstring s(len, L'\0');
        if (len > 0) GetWindowTextW(h, &s[0], len + 1);
        return s;
    }

    std::wstring FaviconUrlFor(std::wstring input) {
        if (input.find(L"://") == std::wstring::npos) input = L"https://" + input;
        size_t schemeEnd = input.find(L"://") + 3;
        size_t pathStart = input.find(L'/', schemeEnd);
        std::wstring origin = (pathStart == std::wstring::npos) ? input : input.substr(0, pathStart);
        return origin + L"/favicon.ico";
    }

    // MurmurHash3 x86-32 (public domain algorithm by Austin Appleby) -
    // Shodan's favicon hash is this function over the Base64 text of the
    // icon's bytes, returned as a signed 32-bit int.
    uint32_t MurmurHash3_x86_32(const void* key, int len, uint32_t seed) {
        const uint8_t* data = (const uint8_t*)key;
        const int nblocks = len / 4;
        uint32_t h1 = seed;
        const uint32_t c1 = 0xcc9e2d51, c2 = 0x1b873593;

        const uint32_t* blocks = (const uint32_t*)(data + nblocks * 4);
        for (int i = -nblocks; i; i++) {
            uint32_t k1;
            memcpy(&k1, blocks + i, sizeof(k1)); // avoid unaligned-read UB
            k1 *= c1; k1 = (k1 << 15) | (k1 >> 17); k1 *= c2;
            h1 ^= k1; h1 = (h1 << 13) | (h1 >> 19); h1 = h1 * 5 + 0xe6546b64;
        }

        const uint8_t* tail = data + nblocks * 4;
        uint32_t k1 = 0;
        switch (len & 3) {
            case 3: k1 ^= tail[2] << 16; [[fallthrough]];
            case 2: k1 ^= tail[1] << 8;  [[fallthrough]];
            case 1: k1 ^= tail[0];
                    k1 *= c1; k1 = (k1 << 15) | (k1 >> 17); k1 *= c2; h1 ^= k1;
        }

        h1 ^= (uint32_t)len;
        h1 ^= h1 >> 16; h1 *= 0x85ebca6b;
        h1 ^= h1 >> 13; h1 *= 0xc2b2ae35;
        h1 ^= h1 >> 16;
        return h1;
    }

    std::string Base64Encode(const std::vector<unsigned char>& data) {
        if (data.empty()) return {};
        DWORD outLen = 0;
        CryptBinaryToStringA(data.data(), (DWORD)data.size(),
            CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &outLen);
        std::string out(outLen, '\0');
        CryptBinaryToStringA(data.data(), (DWORD)data.size(),
            CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &out[0], &outLen);
        if (!out.empty() && out.back() == '\0') out.pop_back();
        return out;
    }
}

void FaviconHashDialog::RegisterClassOnce() {
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

void FaviconHashDialog::ShowFor(HWND owner) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - Favicon Hash (Shodan-style mmh3)",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 480, 260,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(hwnd, SW_SHOW);
}

void FaviconHashDialog::RunHash(HWND hwnd) {
    Controls* c = GetControls(hwnd);
    std::wstring site = GetText(c->url);
    if (site.empty()) return;
    std::wstring faviconUrl = FaviconUrlFor(site);

    SetWindowTextW(c->results, (L"Fetching " + faviconUrl + L" ...").c_str());
    HttpFetchResult r = HttpFetchGet(faviconUrl);
    std::wstringstream out;
    if (!r.ok) {
        out << L"Request failed: " << r.error;
    } else if (r.statusCode != 200 || r.body.empty()) {
        out << L"HTTP " << r.statusCode << L" - no favicon found at " << faviconUrl;
    } else {
        std::string b64 = Base64Encode(r.body);
        // Shodan inserts a newline every 76 chars before hashing (matching
        // Python's base64.encodestring() behavior it was built against) -
        // reproduced here so hashes match Shodan's published values.
        std::string b64WithNewlines;
        for (size_t i = 0; i < b64.size(); i += 76) {
            b64WithNewlines += b64.substr(i, 76);
            b64WithNewlines += '\n';
        }
        int32_t hash = (int32_t)MurmurHash3_x86_32(b64WithNewlines.data(), (int)b64WithNewlines.size(), 0);
        out << L"Favicon size: " << r.body.size() << L" bytes\r\n";
        out << L"mmh3 hash (Shodan http.favicon.hash format): " << hash << L"\r\n\r\n";
        out << L"Search Shodan for: http.favicon.hash:" << hash;
    }
    SetWindowTextW(c->results, out.str().c_str());
}

LRESULT CALLBACK FaviconHashDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            Controls* c = new Controls();
            CreateWindowW(L"STATIC", L"Site (URL or bare host):", WS_CHILD | WS_VISIBLE, 10, 10, 250, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            c->url = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
                10, 30, 340, 24, hwnd, (HMENU)(INT_PTR)ID_URL, GetModuleHandleW(nullptr), nullptr);
            CreateWindowW(L"BUTTON", L"Hash", WS_CHILD | WS_VISIBLE, 360, 30, 100, 24,
                hwnd, (HMENU)(INT_PTR)ID_HASH, GetModuleHandleW(nullptr), nullptr);
            c->results = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL, 10, 64, 450, 150,
                hwnd, (HMENU)(INT_PTR)ID_RESULTS, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HWND kids[] = { c->url, c->results };
            for (auto k : kids) SendMessageW(k, WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)c);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_HASH && HIWORD(wParam) == BN_CLICKED) RunHash(hwnd);
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
