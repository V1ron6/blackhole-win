#pragma once
#include <windows.h>

// Port of StringsExtractor.kt. Extracts runs of printable ASCII characters
// (minimum length 4, same default as the Unix `strings` utility) from a
// local file - purely offline/local, no network.
class StringsExtractorDialog {
public:
    static void ShowFor(HWND owner);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
    static void PickAndExtract(HWND hwnd);
};
