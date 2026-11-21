#pragma once
#include <windows.h>

// Port of RegexTester.kt. Tests a pattern against sample text using
// std::wregex (ECMAScript grammar, same flavor Java/Kotlin's regex and
// JavaScript both use, so patterns written for the Android app's tester
// should behave the same way here).
class RegexTesterDialog {
public:
    static void ShowFor(HWND owner);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
    static void RunTest(HWND hwnd);
};
