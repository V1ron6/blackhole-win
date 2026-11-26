#include "HashCrackerDialog.h"
#include "WordlistLoader.h"
#include <bcrypt.h>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <sstream>
#include <algorithm>
#include <cwctype>

#pragma comment(lib, "bcrypt.lib")

namespace {
    constexpr int ID_HASH_INPUT = 3501, ID_HASH_TYPE = 3502, ID_WORDLIST_BUILTIN = 3503,
                  ID_WORDLIST_CUSTOM = 3504, ID_CUSTOM_BOX = 3505, ID_CHK_CAP = 3506,
                  ID_CHK_LEET = 3507, ID_CHK_DIGITS = 3508, ID_START = 3509,
                  ID_STATUS = 3510, ID_RESULT = 3511;
    constexpr UINT WM_CRACK_PROGRESS = WM_APP + 1;
    constexpr UINT WM_CRACK_DONE     = WM_APP + 2;
    constexpr int kMaxAttemptsFast = 150000;
    constexpr int kProgressEvery = 500;
    const wchar_t* kClassName = L"BlackholeHashCracker";
    const wchar_t* kHashTypes[] = { L"MD5", L"SHA-1", L"SHA-256", L"SHA-512", L"NTLM", L"bcrypt" };

    struct State {
        HWND hashInput, hashType, wordlistBuiltin, wordlistCustom, customBox,
             chkCap, chkLeet, chkDigits, start, status, result;
        std::shared_ptr<std::atomic<bool>> running = std::make_shared<std::atomic<bool>>(false);
    };
    State* GetState(HWND h) { return reinterpret_cast<State*>(GetWindowLongPtr(h, GWLP_USERDATA)); }

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

    std::wstring HashHexCng(const std::string& data, LPCWSTR algId) {
        BCRYPT_ALG_HANDLE hAlg = nullptr;
        if (BCryptOpenAlgorithmProvider(&hAlg, algId, nullptr, 0) != 0) return L"";
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
        for (UCHAR b : hash) { wchar_t buf[3]; wsprintfW(buf, L"%02x", b); hex << buf; }
        return hex.str();
    }

    // NTLM = MD4 of the UTF-16LE password bytes, no salt - same as
    // HashCracker.kt's ntlmHex(). CNG's base provider includes MD4.
    std::wstring NtlmHex(const std::wstring& password) {
        BCRYPT_ALG_HANDLE hAlg = nullptr;
        if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_MD4_ALGORITHM, nullptr, 0) != 0) return L"";
        DWORD hashLen = 0, cbResult = 0;
        BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashLen, sizeof(hashLen), &cbResult, 0);
        std::vector<UCHAR> hash(hashLen);
        BCRYPT_HASH_HANDLE hHash = nullptr;
        BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
        BCryptHashData(hHash, (PUCHAR)password.data(), (ULONG)(password.size() * sizeof(wchar_t)), 0);
        BCryptFinishHash(hHash, hash.data(), hashLen, 0);
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        std::wstringstream hex;
        for (UCHAR b : hash) { wchar_t buf[3]; wsprintfW(buf, L"%02x", b); hex << buf; }
        return hex.str();
    }

    bool EqualsIgnoreCase(const std::wstring& a, const std::wstring& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) if (towlower(a[i]) != towlower(b[i])) return false;
        return true;
    }

    // Verifies one candidate against the target hash for the selected type.
    // bcrypt is NOT implemented: it's a salted, iterated Blowfish-based KDF
    // (the original Kotlin used BouncyCastle's OpenBSDBCrypt.checkPassword).
    // Hand-rolling a correct, constant-behavior Blowfish+bcrypt from scratch
    // without a crypto library to check it against was judged too large and
    // too easy to get subtly wrong for this stage - Windows CNG has no
    // built-in bcrypt-the-password-hash primitive (BCrypt* the API is a
    // different, unrelated thing from bcrypt the password hash). Selecting
    // "bcrypt" in the dropdown shows this explanation instead of either
    // crashing or silently reporting false negatives.
    bool Verify(const std::wstring& type, const std::wstring& candidate, const std::wstring& target) {
        if (type == L"MD5")     return EqualsIgnoreCase(HashHexCng(ToUtf8(candidate), BCRYPT_MD5_ALGORITHM), target);
        if (type == L"SHA-1")   return EqualsIgnoreCase(HashHexCng(ToUtf8(candidate), BCRYPT_SHA1_ALGORITHM), target);
        if (type == L"SHA-256") return EqualsIgnoreCase(HashHexCng(ToUtf8(candidate), BCRYPT_SHA256_ALGORITHM), target);
        if (type == L"SHA-512") return EqualsIgnoreCase(HashHexCng(ToUtf8(candidate), BCRYPT_SHA512_ALGORITHM), target);
        if (type == L"NTLM")    return EqualsIgnoreCase(NtlmHex(candidate), target);
        return false; // bcrypt - never matches, see Verify's doc comment
    }

    std::wstring Leetify(std::wstring w) {
        for (auto& c : w) {
            switch (c) {
                case L'a': case L'A': c = L'4'; break;
                case L'e': case L'E': c = L'3'; break;
                case L'i': case L'I': c = L'1'; break;
                case L'o': case L'O': c = L'0'; break;
                case L's': case L'S': c = L'5'; break;
            }
        }
        return w;
    }

    std::wstring Capitalize(std::wstring w) {
        if (!w.empty()) w[0] = towupper(w[0]);
        return w;
    }
}

void HashCrackerDialog::RegisterClassOnce() {
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

void HashCrackerDialog::ShowFor(HWND owner) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - Hash Cracker (JtR-lite)",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 560, 520,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(hwnd, SW_SHOW);
}

void HashCrackerDialog::ToggleCracking(HWND hwnd) {
    State* st = GetState(hwnd);
    if (st->running->load()) {
        *st->running = false; // background thread notices on its next check and stops
        return;
    }

    std::wstring target = GetText(st->hashInput);
    size_t a = target.find_first_not_of(L" \t");
    size_t b = target.find_last_not_of(L" \t");
    target = (a == std::wstring::npos) ? L"" : target.substr(a, b - a + 1);
    if (target.empty()) { SetWindowTextW(st->status, L"Enter a hash first."); return; }

    int typeIndex = (int)SendMessageW(st->hashType, CB_GETCURSEL, 0, 0);
    std::wstring type = kHashTypes[typeIndex < 0 ? 0 : typeIndex];
    if (type == L"bcrypt") {
        SetWindowTextW(st->status,
            L"bcrypt verification isn't implemented in this Windows build "
            L"(Windows CNG has no bcrypt-the-password-hash primitive; see "
            L"docs/PORTING_NOTES.md). Pick a different hash type.");
        return;
    }

    bool useCustom = SendMessageW(st->wordlistCustom, BM_GETCHECK, 0, 0) == BST_CHECKED;
    std::vector<std::wstring> words = useCustom
        ? ParseWordlistText(GetText(st->customBox))
        : LoadWordlistFile(ExeDirectory() + L"\\assets\\common_passwords.txt");
    if (words.empty()) { SetWindowTextW(st->status, L"Wordlist is empty."); return; }

    bool cap = SendMessageW(st->chkCap, BM_GETCHECK, 0, 0) == BST_CHECKED;
    bool leet = SendMessageW(st->chkLeet, BM_GETCHECK, 0, 0) == BST_CHECKED;
    bool digits = SendMessageW(st->chkDigits, BM_GETCHECK, 0, 0) == BST_CHECKED;

    *st->running = true;
    SetWindowTextW(st->start, L"Stop");
    SetWindowTextW(st->result, L"");
    SetWindowTextW(st->status, L"Cracking\u2026");

    auto running = st->running;
    std::thread([hwnd, running, target, type, words, cap, leet, digits]() {
        int attempts = 0;
        std::wstring found;
        bool stoppedEarly = false;

        for (auto& word : words) {
            if (!running->load()) { stoppedEarly = true; break; }
            // Same candidate order as candidateSequence() in HashCracker.kt:
            // base word, then capitalize, then leet, then word+0..99.
            std::vector<std::wstring> candidates = { word };
            if (cap) candidates.push_back(Capitalize(word));
            if (leet) candidates.push_back(Leetify(word));
            if (digits) for (int n = 0; n <= 99; ++n) candidates.push_back(word + std::to_wstring(n));

            bool hit = false;
            for (auto& candidate : candidates) {
                if (!running->load() || attempts >= kMaxAttemptsFast) { stoppedEarly = !hit; break; }
                attempts++;
                if (Verify(type, candidate, target)) { found = candidate; hit = true; break; }
                if (attempts % kProgressEvery == 0) {
                    auto* msg = new std::wstring(L"Tried " + std::to_wstring(attempts) + L" candidate(s)\u2026");
                    PostMessageW(hwnd, WM_CRACK_PROGRESS, 0, (LPARAM)msg);
                }
            }
            if (hit || attempts >= kMaxAttemptsFast) break;
        }

        *running = false;
        auto* result = new std::pair<std::wstring, int>(found, attempts);
        PostMessageW(hwnd, WM_CRACK_DONE, stoppedEarly && found.empty() ? 1 : 0, (LPARAM)result);
    }).detach();
}

LRESULT CALLBACK HashCrackerDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            State* st = new State();
            CreateWindowW(L"STATIC", L"Hash:", WS_CHILD | WS_VISIBLE, 10, 10, 60, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            st->hashInput = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
                70, 8, 300, 24, hwnd, (HMENU)(INT_PTR)ID_HASH_INPUT, GetModuleHandleW(nullptr), nullptr);
            st->hashType = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                380, 8, 150, 150, hwnd, (HMENU)(INT_PTR)ID_HASH_TYPE, GetModuleHandleW(nullptr), nullptr);
            for (auto t : kHashTypes) SendMessageW(st->hashType, CB_ADDSTRING, 0, (LPARAM)t);
            SendMessageW(st->hashType, CB_SETCURSEL, 0, 0);

            st->wordlistBuiltin = CreateWindowW(L"BUTTON", L"Built-in wordlist (1000 passwords)",
                WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP, 10, 46, 260, 20,
                hwnd, (HMENU)(INT_PTR)ID_WORDLIST_BUILTIN, GetModuleHandleW(nullptr), nullptr);
            st->wordlistCustom = CreateWindowW(L"BUTTON", L"Custom wordlist",
                WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 280, 46, 150, 20,
                hwnd, (HMENU)(INT_PTR)ID_WORDLIST_CUSTOM, GetModuleHandleW(nullptr), nullptr);
            SendMessageW(st->wordlistBuiltin, BM_SETCHECK, BST_CHECKED, 0);
            st->customBox = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_BORDER | ES_MULTILINE | WS_VSCROLL,
                10, 70, 520, 70, hwnd, (HMENU)(INT_PTR)ID_CUSTOM_BOX, GetModuleHandleW(nullptr), nullptr);

            st->chkCap = CreateWindowW(L"BUTTON", L"Capitalize", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                10, 148, 110, 20, hwnd, (HMENU)(INT_PTR)ID_CHK_CAP, GetModuleHandleW(nullptr), nullptr);
            st->chkLeet = CreateWindowW(L"BUTTON", L"Leetspeak", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                130, 148, 110, 20, hwnd, (HMENU)(INT_PTR)ID_CHK_LEET, GetModuleHandleW(nullptr), nullptr);
            st->chkDigits = CreateWindowW(L"BUTTON", L"Append 0-99", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                250, 148, 120, 20, hwnd, (HMENU)(INT_PTR)ID_CHK_DIGITS, GetModuleHandleW(nullptr), nullptr);

            st->start = CreateWindowW(L"BUTTON", L"Start Cracking", WS_CHILD | WS_VISIBLE, 10, 178, 140, 28,
                hwnd, (HMENU)(INT_PTR)ID_START, GetModuleHandleW(nullptr), nullptr);
            st->status = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 10, 214, 520, 20,
                hwnd, (HMENU)(INT_PTR)ID_STATUS, GetModuleHandleW(nullptr), nullptr);
            st->result = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY,
                10, 238, 520, 24, hwnd, (HMENU)(INT_PTR)ID_RESULT, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HWND kids[] = { st->hashInput, st->hashType, st->wordlistBuiltin, st->wordlistCustom, st->customBox,
                             st->chkCap, st->chkLeet, st->chkDigits, st->start, st->status, st->result };
            for (auto k : kids) SendMessageW(k, WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)st);
            return 0;
        }
        case WM_COMMAND: {
            State* st = GetState(hwnd);
            if ((LOWORD(wParam) == ID_WORDLIST_BUILTIN || LOWORD(wParam) == ID_WORDLIST_CUSTOM) &&
                HIWORD(wParam) == BN_CLICKED) {
                bool custom = SendMessageW(st->wordlistCustom, BM_GETCHECK, 0, 0) == BST_CHECKED;
                ShowWindow(st->customBox, custom ? SW_SHOW : SW_HIDE);
                return 0;
            }
            if (LOWORD(wParam) == ID_START && HIWORD(wParam) == BN_CLICKED) {
                ToggleCracking(hwnd);
                return 0;
            }
            return 0;
        }
        case WM_CRACK_PROGRESS: {
            State* st = GetState(hwnd);
            std::unique_ptr<std::wstring> text((std::wstring*)lParam);
            SetWindowTextW(st->status, text->c_str());
            return 0;
        }
        case WM_CRACK_DONE: {
            State* st = GetState(hwnd);
            std::unique_ptr<std::pair<std::wstring, int>> r((std::pair<std::wstring, int>*)lParam);
            bool wasStopped = (wParam == 1);
            SetWindowTextW(st->start, L"Start Cracking");
            if (!r->first.empty()) {
                SetWindowTextW(st->status, (L"Found after " + std::to_wstring(r->second) + L" attempt(s).").c_str());
                SetWindowTextW(st->result, (L"Password: " + r->first).c_str());
            } else if (wasStopped) {
                SetWindowTextW(st->status, (L"Stopped after " + std::to_wstring(r->second) + L" attempt(s).").c_str());
                SetWindowTextW(st->result, L"");
            } else {
                SetWindowTextW(st->status, (L"Not found in " + std::to_wstring(r->second) + L" attempt(s).").c_str());
                SetWindowTextW(st->result, L"");
            }
            return 0;
        }
        case WM_DESTROY: {
            State* st = GetState(hwnd);
            *st->running = false;
            delete st;
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
