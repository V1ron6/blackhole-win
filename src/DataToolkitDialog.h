#pragma once
#include <windows.h>

// Port of DataToolkit.kt: Base64 encode/decode, hex encode/decode, and
// MD5/SHA-1/SHA-256 hashing of arbitrary input text. Uses Windows CNG
// (bcrypt.lib) for hashing instead of javax.crypto.MessageDigest, and
// CryptBinaryToString/CryptStringToBinary for Base64 instead of
// android.util.Base64.
class DataToolkitDialog {
public:
    static void ShowFor(HWND owner);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
    static void RunTransform(HWND hwnd, int action);
};
