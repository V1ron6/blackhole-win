#pragma once
#include <string>
#include <thread>
#include <atomic>

// Port of DownloadManager.kt. Files land in
// %USERPROFILE%\Downloads\blackhole\ and a background thread sweeps that
// folder every few minutes, deleting anything older than the configured
// retention window - same "expiring downloads" behavior as the Android app.
class DownloadManager {
public:
    ~DownloadManager();

    std::wstring DownloadsFolder(); // creates it if missing
    void StartRetentionSweeper(int retentionHours);
    void Stop();

    // Stage 2: used by BlackholeApp's ICoreWebView2::add_DownloadStarting
    // handler to force every download into DownloadsFolder() instead of
    // wherever WebView2's default download UI would otherwise put it -
    // this header stays free of WebView2 types so only BlackholeApp.cpp
    // needs to know about ICoreWebView2DownloadStartingEventArgs.
    // Collisions (same filename twice) get " (1)", " (2)", etc. appended,
    // same as a normal browser's download manager.
    std::wstring ResultPathFor(const std::wstring& suggestedFileName);

private:
    std::thread sweeper_;
    std::atomic<bool> running_{false};
    std::atomic<int> retentionHours_{24};
};
