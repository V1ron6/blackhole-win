#pragma once
#include <windows.h>

// Port of HashCracker.kt ("JtR-lite"). Offline dictionary + basic-rule hash
// cracker: MD5/SHA-1/SHA-256/SHA-512/NTLM via a plain digest comparison
// (Windows CNG), same mutation rules as the original (capitalize, leetspeak,
// append 0-99). bcrypt is listed but not functional in this build - see
// the header comment on BcryptVerify() in the .cpp for why, instead of
// silently failing or pretending it works.
class HashCrackerDialog {
public:
    static void ShowFor(HWND owner);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
    static void ToggleCracking(HWND hwnd);
};
