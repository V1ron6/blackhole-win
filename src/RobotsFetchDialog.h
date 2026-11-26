#pragma once
#include <windows.h>

// Port of RobotsFetch.kt. Fetches and displays a site's /robots.txt -
// public, standard, no different from loading it in a browser tab.
class RobotsFetchDialog {
public:
    static void ShowFor(HWND owner);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
    static void Fetch(HWND hwnd);
};
