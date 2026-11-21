#pragma once
#include <windows.h>

// Port of PostmanRequestDialog.kt + HttpMethod.kt (Intermediate mode).
// A plain Win32 modeless window (no .rc dialog template needed) with a
// method dropdown, URL field, headers box, body box, a Send button and a
// response viewer. Uses WinHTTP directly rather than WebView2, since this
// tool deliberately bypasses the page context (same as the Kotlin version,
// which used HttpURLConnection instead of the WebView).
class PostmanDialog {
public:
    static void ShowFor(HWND owner);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void SendRequest(HWND hwnd);
    static void RegisterClassOnce();
};
