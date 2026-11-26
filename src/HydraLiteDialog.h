#pragma once
#include <windows.h>

// Port of HydraLite.kt. A small credential brute forcer covering HTTP
// Basic Auth, HTTP POST login forms, FTP, and SSH - same four protocols,
// same hard cap (500 total attempts), same lower concurrency (5) than
// PortScanner/DirectoryBuster since this attacks a live service where
// real accounts can get locked out, same authorization-checkbox gate, and
// same "stop at the first working credential pair" behavior rather than
// enumerating everything. The SSH protocol module needs libssh2 (see
// SshClient.h) - the other three protocols work in every build.
class HydraLiteDialog {
public:
    static void ShowFor(HWND owner);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
    static void ToggleAttack(HWND hwnd);
    static void UpdateFieldVisibility(HWND hwnd);
};
