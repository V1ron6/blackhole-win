#pragma once
#include <windows.h>
#include <functional>
#include <string>

// Port of ScreenshotCapture.kt. Captures the active tab's current view as
// a PNG via ICoreWebView2::CapturePreview - direct equivalent of the
// Android version's View.draw(canvas)-based screenshot.
class ScreenshotCaptureDialog {
public:
    // capture(path, callback(success, pathOrError)) - wired to
    // BlackholeApp::CaptureScreenshotOfActiveTab.
    using CaptureFn = std::function<void(const std::wstring&, std::function<void(bool, const std::wstring&)>)>;
    static void ShowFor(HWND owner, CaptureFn capture);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
    static void RunCapture(HWND hwnd);
};
