#pragma once
#include <windows.h>

// Port of TechFingerprint.kt. Heuristic tech-stack detection from response
// headers and page markup (same idea as Wappalyzer) - pattern matching on
// publicly-served content, no exploitation.
class TechFingerprintDialog {
public:
    static void ShowFor(HWND owner);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
    static void RunScan(HWND hwnd);
};
