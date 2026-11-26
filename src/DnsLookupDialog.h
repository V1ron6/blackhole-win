#pragma once
#include <windows.h>

// Port of DnsLookup.kt. Queries A, AAAA, MX, TXT, NS and CNAME records via
// the OS resolver (DnsQuery_W) - same records the Android version's
// java.net/dnsjava-based lookup covered.
class DnsLookupDialog {
public:
    static void ShowFor(HWND owner);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
    static void RunLookup(HWND hwnd);
};
