#pragma once
#include <windows.h>

// Port of DiffViewer.kt. Line-based diff via a classic LCS (longest common
// subsequence) dynamic-programming table - O(n*m), fine for the pasted-text
// sizes this dialog is meant for, same algorithmic approach most simple
// text-diff utilities use.
class DiffViewerDialog {
public:
    static void ShowFor(HWND owner);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
    static void RunDiff(HWND hwnd);
};
