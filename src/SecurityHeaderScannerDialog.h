#pragma once
#include <windows.h>

// Port of SecurityHeaderScanner.kt. Fetches a URL and checks for the
// standard response security headers - same checklist idea as the public
// securityheaders.com, reading response headers only (no exploitation).
class SecurityHeaderScannerDialog {
public:
    static void ShowFor(HWND owner);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
    static void RunScan(HWND hwnd);
};
