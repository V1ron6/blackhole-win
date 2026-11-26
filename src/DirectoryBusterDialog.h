#pragma once
#include <windows.h>

// Port of DirectoryBuster.kt. A small, deliberately lightweight
// directory/file fuzzer (gobuster's basic idea - try a wordlist of paths,
// report what isn't a 404). Same safety rails as the original: built-in
// wordlist only (~185 entries, assets/common_paths.txt), custom wordlist
// hard-capped at MAX_TOTAL_REQUESTS candidates, bounded concurrency,
// response bodies never read (only the status line), redirects not
// auto-followed, and the Scan button stays gated behind an authorization
// checkbox exactly like PortScanner's.
class DirectoryBusterDialog {
public:
    static void ShowFor(HWND owner);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
    static void StartScan(HWND hwnd);
};
