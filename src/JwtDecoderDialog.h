#pragma once
#include <windows.h>

// Port of JwtDecoder.kt. Decodes (does NOT verify) a JWT's header and
// payload segments - same scope as the Kotlin version and as jwt.io's
// "debugger" view: it reads the Base64URL-encoded JSON, it does not check
// the signature against any key.
class JwtDecoderDialog {
public:
    static void ShowFor(HWND owner);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
    static void Decode(HWND hwnd);
};
