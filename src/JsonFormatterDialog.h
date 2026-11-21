#pragma once
#include <windows.h>

// Port of JsonFormatter.kt. Pretty-prints or minifies JSON text. This is a
// structural reformatter (tracks string literals/escapes and brace/bracket
// depth), not a validating parser - like the Kotlin version, malformed
// input gets a best-effort reformat rather than a hard parse error.
class JsonFormatterDialog {
public:
    static void ShowFor(HWND owner);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
    static void Run(HWND hwnd, bool pretty);
};
