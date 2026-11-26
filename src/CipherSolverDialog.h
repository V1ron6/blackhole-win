#pragma once
#include <windows.h>

// Port of CipherSolver.kt. Classic-cipher transforms: ROT13, Atbash, and a
// brute-force listing of all 26 Caesar shifts (so the user can eyeball
// which one produces readable text) - same scope as the Kotlin version,
// no frequency-analysis auto-solve.
class CipherSolverDialog {
public:
    static void ShowFor(HWND owner);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
    static void Run(HWND hwnd, int action);
};
