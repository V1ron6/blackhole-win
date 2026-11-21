#pragma once
#include <windows.h>

// Port of HashIdentifier.kt. Pattern-matches a hash string's length/charset
// against known hash formats and lists plausible candidates - this does
// NOT crack or verify anything (see HashCracker.kt in PORTING_NOTES.md for
// the cracking tool, which is a separate, not-yet-ported, flagged item).
class HashIdentifierDialog {
public:
    static void ShowFor(HWND owner);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
    static void Identify(HWND hwnd);
};
