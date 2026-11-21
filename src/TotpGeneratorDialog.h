#pragma once
#include <windows.h>

// Port of TotpGenerator.kt. Standard RFC 6238 TOTP (HMAC-SHA1, 30-second
// step, 6 digits) from a Base32 secret, same as the "scan this QR code"
// secret any authenticator app uses. Auto-refreshes once a second via a
// Win32 timer instead of a Kotlin coroutine loop.
class TotpGeneratorDialog {
public:
    static void ShowFor(HWND owner);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
    static void Tick(HWND hwnd);
};
