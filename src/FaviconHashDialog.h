#pragma once
#include <windows.h>

// Port of FaviconHash.kt. Fetches /favicon.ico, Base64-encodes it, then
// runs MurmurHash3 x86-32 over the Base64 text - the same technique
// Shodan's http.favicon.hash field uses and documents publicly. Useful for
// finding other sites sharing a favicon (e.g. the same CMS/framework
// default icon, or infrastructure fingerprinting in a CTF).
class FaviconHashDialog {
public:
    static void ShowFor(HWND owner);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
    static void RunHash(HWND hwnd);
};
