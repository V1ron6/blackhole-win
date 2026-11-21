#pragma once
#include <string>
#include <cstdint>

// Mirrors blackhole-main/app/src/main/java/com/blackhole/browser/BrowserMode.kt
// and Settings.kt. Kept as a flat struct + plain-text key=value file
// (no JSON dependency needed) at %APPDATA%\Blackhole\settings.ini.
enum class BrowserMode { Basic = 0, Intermediate = 1, Advance = 2, ByteBandit = 3 };
enum class SecurityMode { Strict = 0, Ctf = 1 };

struct AppSettings {
    BrowserMode   browserMode      = BrowserMode::Basic;
    SecurityMode  securityMode     = SecurityMode::Strict;

    bool          proxyEnabled     = false;
    std::wstring  proxyScheme      = L"socks5"; // socks5 | http
    std::wstring  proxyHost        = L"127.0.0.1";
    int           proxyPort        = 9050;       // Orbot default

    int           downloadRetentionHours = 24;

    bool          notificationsEnabled = true;

    static std::wstring SettingsFilePath();
    static AppSettings  Load();
    bool                Save() const;

    // Convenience presets mirroring the Kotlin "Burp Suite quick-fill" button.
    void ApplyBurpPreset() {
        proxyEnabled = true;
        proxyScheme  = L"http";
        proxyHost    = L"127.0.0.1";
        proxyPort    = 8080;
    }

    // Matches ProxyManager.kt: returns the --proxy-server switch value
    // passed to the WebView2 environment at creation time.
    std::wstring ProxyServerSwitchValue() const {
        return proxyScheme + L"://" + proxyHost + L":" + std::to_wstring(proxyPort);
    }
};

const wchar_t* BrowserModeName(BrowserMode m);
const wchar_t* SecurityModeName(SecurityMode m);
