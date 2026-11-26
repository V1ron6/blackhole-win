#pragma once
#include <windows.h>

// Port of PortScanner.kt. TCP connect-scan against the same 29 common
// ports the original used. The Scan button stays disabled until the
// "I am authorized to scan this host" checkbox is ticked - preserved
// verbatim from the Kotlin version's own doc comment: "not a legal
// safeguard, just a UI speed bump against scanning something by accident
// or habit." Bounded concurrency via ConcurrentScan.h's RunConcurrent.
class PortScannerDialog {
public:
    static void ShowFor(HWND owner);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
    static void StartScan(HWND hwnd);
};
