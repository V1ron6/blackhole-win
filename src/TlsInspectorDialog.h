#pragma once
#include <windows.h>

// Port of TlsInspector.kt. Connects to an https:// URL and reports the
// negotiated TLS protocol, cipher/signature algorithm names, and the
// server certificate's subject/issuer/validity dates - via WinHTTP's
// WINHTTP_OPTION_SECURITY_CERTIFICATE_STRUCT, which is the direct
// equivalent of the Android version reading the SSLSession/X509Certificate
// off an HttpsURLConnection.
class TlsInspectorDialog {
public:
    static void ShowFor(HWND owner);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
    static void Inspect(HWND hwnd);
};
