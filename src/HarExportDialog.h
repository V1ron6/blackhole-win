#pragma once
#include <windows.h>
#include <functional>
#include <string>

// Port of HarExport.kt. Exports the active tab's request log as a minimal
// but valid .har (HTTP Archive) file. Simplification, stated plainly: the
// current request log (Tab::requestLog, captured in
// BlackholeApp::HookAdBlocking) only has URL + allowed/blocked status, not
// per-request timing/headers/bodies - so entries use placeholder/zeroed
// timing fields rather than fabricated numbers. A full HAR would need
// WebResourceRequested to also capture start/end timestamps and header
// data per request, which is a reasonable Stage 6+ enhancement if the HAR
// output needs to be richer (e.g. for importing into Chrome DevTools).
class HarExportDialog {
public:
    using GetLogFn = std::function<std::wstring()>;
    static void ShowFor(HWND owner, GetLogFn getLog);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
    static void Export(HWND hwnd);
};
