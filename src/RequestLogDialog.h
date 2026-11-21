#pragma once
#include <windows.h>
#include <functional>
#include <string>

// Stage 2: viewer for Tab::requestLog (RequestLogEntry.kt's per-tab,
// BLOCKED/allowed, newest-first, 200-entry-capped log). The log itself is
// already captured in BlackholeApp.cpp's WebResourceRequested handler -
// this dialog just needed a place to show it, same as the Android
// history-icon button did.
class RequestLogDialog {
public:
    // getLatest is called once on open and again each time Refresh is
    // clicked, so the dialog always shows the current state of the tab's
    // log without needing a live callback wired up.
    static void ShowFor(HWND owner, std::function<std::wstring()> getLatest);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
};
