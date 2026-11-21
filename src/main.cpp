#include <windows.h>
#include "BlackholeApp.h"

// Pulls in the Common Controls v6 manifest (themed buttons/edit controls)
// without needing a separate .rc/.manifest file - same visual upgrade the
// Android version got for free from Material theming.
#pragma comment(linker,"\"/manifestdependency:type='win32' "\
    "name='Microsoft.Windows.Common-Controls' version='6.0.0.0' "\
    "processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    // WebView2 and WinHTTP both want COM initialized on the UI thread.
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    BlackholeApp app;
    if (!app.Initialize(hInstance, nCmdShow)) {
        MessageBoxW(nullptr,
            L"Blackhole could not start. The most common cause is a missing "
            L"WebView2 Runtime - see docs/BUILD_WINDOWS.md.",
            L"Blackhole", MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    int code = app.RunMessageLoop();
    CoUninitialize();
    return code;
}
