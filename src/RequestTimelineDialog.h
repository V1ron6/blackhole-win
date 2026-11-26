#pragma once
#include <windows.h>
#include <functional>
#include <string>

// Port of RequestTimeline.kt. Shows the active tab's request log in
// chronological (oldest-first) order with wall-clock timestamps - a
// simplification worth stating plainly: Tab::requestLog captures a
// timestamp and BLOCKED/allowed status per request (added in Stage 5
// specifically for this dialog) but not per-request duration/latency, so
// this is a chronological list, not a graphical waterfall with bar widths
// proportional to load time. A true waterfall would need
// WebResourceRequested paired with a completion event to measure duration,
// which the current hook doesn't do.
class RequestTimelineDialog {
public:
    using GetLogFn = std::function<std::wstring()>;
    static void ShowFor(HWND owner, GetLogFn getLog);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
};
