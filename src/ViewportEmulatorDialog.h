#pragma once
#include <windows.h>
#include <functional>

// Port of ViewportEmulator.kt. Resizes the active tab's WebView2 controller
// to common device sizes - a crude viewport emulator (shrinks the browser
// content area, pinned top-left; no scaling/device-frame chrome like
// Chrome DevTools draws). Wired to BlackholeApp::SetViewportOverrideForActiveTab
// / ClearViewportOverrideForActiveTab.
class ViewportEmulatorDialog {
public:
    using SetFn = std::function<void(int width, int height)>;
    using ClearFn = std::function<void()>;
    static void ShowFor(HWND owner, SetFn setViewport, ClearFn clearViewport);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
};
