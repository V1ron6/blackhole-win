#include "DownloadManager.h"
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <chrono>

namespace fs = std::filesystem;

std::wstring DownloadManager::DownloadsFolder() {
    PWSTR dl = nullptr;
    std::wstring path = L".\\downloads";
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &dl))) {
        path = std::wstring(dl) + L"\\blackhole";
        CoTaskMemFree(dl);
    }
    fs::create_directories(path);
    return path;
}

void DownloadManager::StartRetentionSweeper(int retentionHours) {
    retentionHours_ = retentionHours;
    if (running_.exchange(true)) return; // already running

    sweeper_ = std::thread([this]() {
        using namespace std::chrono_literals;
        while (running_) {
            std::wstring folder = DownloadsFolder();
            auto cutoff = std::chrono::system_clock::now() -
                          std::chrono::hours(retentionHours_.load());

            std::error_code ec;
            if (fs::exists(folder, ec)) {
                for (auto& entry : fs::directory_iterator(folder, ec)) {
                    if (!entry.is_regular_file(ec)) continue;
                    auto ftime = fs::last_write_time(entry, ec);
                    if (ec) continue;
                    // Rough but good-enough clock conversion for a
                    // retention sweep (exact epoch alignment doesn't
                    // matter at hour granularity).
                    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
                    if (sctp < cutoff) {
                        fs::remove(entry, ec);
                    }
                }
            }

            for (int i = 0; i < 300 && running_; ++i) std::this_thread::sleep_for(1s); // check every 5 min
        }
    });
}

std::wstring DownloadManager::ResultPathFor(const std::wstring& suggestedFileName) {
    std::wstring folder = DownloadsFolder();
    std::wstring name = suggestedFileName.empty() ? L"download" : suggestedFileName;

    // Split "name.ext" so collision suffixes land before the extension
    // ("report (1).pdf"), not after it ("report.pdf (1)").
    size_t dot = name.find_last_of(L'.');
    std::wstring stem = (dot == std::wstring::npos) ? name : name.substr(0, dot);
    std::wstring ext  = (dot == std::wstring::npos) ? L""   : name.substr(dot);

    std::wstring candidate = folder + L"\\" + name;
    std::error_code ec;
    int suffix = 1;
    while (fs::exists(candidate, ec)) {
        candidate = folder + L"\\" + stem + L" (" + std::to_wstring(suffix++) + L")" + ext;
    }
    return candidate;
}

void DownloadManager::Stop() {
    running_ = false;
    if (sweeper_.joinable()) sweeper_.join();
}

DownloadManager::~DownloadManager() { Stop(); }
