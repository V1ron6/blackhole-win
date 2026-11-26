#pragma once
#include <windows.h>

// Port of WhoisLookup.kt. Raw TCP socket to a WHOIS server's port 43 -
// same protocol-level approach the Kotlin version used (java.net.Socket),
// just Winsock instead. WHOIS has no universal server; IANA's server will
// usually point you at the right registry-specific server in its reply
// (the "Registrar WHOIS Server:" or "whois:" line), which you then paste
// into the Server field for a second, more detailed query - same two-step
// flow a `whois` CLI does automatically and this simple version doesn't.
class WhoisLookupDialog {
public:
    static void ShowFor(HWND owner);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
    static void RunQuery(HWND hwnd);
};
