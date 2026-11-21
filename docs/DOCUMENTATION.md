# Blackhole for Windows — Documentation

## Architecture overview

```
main.cpp            wWinMain: COM init, creates BlackholeApp, runs message loop
BlackholeApp.h/.cpp  Main window: toolbar, tab management, WebView2 wiring,
                     ad blocking, security-mode enforcement, Settings window
Settings.h/.cpp      AppSettings struct + load/save to %APPDATA%\Blackhole\settings.ini
AdBlocker.h/.cpp     Host-based request blocker (reads assets\adblock_hosts.txt)
DownloadManager.h/.cpp  Downloads folder + background retention sweeper thread
PostmanDialog.h/.cpp    Intermediate-mode HTTP request tool (WinHTTP)
JsConsoleDialog.h/.cpp  Advance-mode JS console (ExecuteScript on active tab)
DataToolkitDialog.h/.cpp  Base64 / hex / MD5 / SHA-1 / SHA-256 utility (CNG)
```

One `BlackholeApp` instance owns:
- The main `HWND` and all toolbar child controls (back/forward/reload/home,
  URL bar, Go, new/close tab, Tools menu, Settings button, mode label).
- Up to **2** `Tab` structs (`kMaxTabs = 2`, matching `TabManager.kt`'s hard
  cap), each wrapping one `ICoreWebView2Controller` + `ICoreWebView2`.
- One shared `ICoreWebView2Environment` (created once at startup; its
  `--proxy-server` argument is fixed for the process's lifetime, which is
  why changing the proxy in Settings asks for a restart).

## Feature mapping (Kotlin → C++)

| Feature | Android file | Windows file | Status |
|---|---|---|---|
| 2-tab cap | `TabManager.kt` | `BlackholeApp.cpp` (`kMaxTabs`) | ✅ Ported |
| WebView hardening | `SecureWebView.kt` | `BlackholeApp.cpp` (`AddTab` callback) | ✅ Ported (JS off by default, no cookies/cache — see note below) |
| Ad blocking | `AdBlocker.kt` | `AdBlocker.h/.cpp` | ✅ Ported (same hosts-file format, same parent-domain matching) |
| Browser modes | `BrowserMode.kt`, `ModeManager.kt` | `Settings.h` (`BrowserMode` enum), gating in `OpenToolForCurrentMode` | ✅ Ported |
| Security mode (Strict/CTF) | part of `Settings.kt` + `SecureWebView.kt` | `BlackholeApp.cpp` (`NavigationStarting` handler) | ✅ Ported |
| Proxy / Tor routing | `ProxyManager.kt` | `CreateWebViewEnvironment()` (`--proxy-server` browser argument) | ⚠️ Ported but **not live-togglable** — WebView2's proxy is fixed at environment creation, unlike Android's `ProxyController` which could flip it mid-session. Changing it in Settings now requires an app restart. |
| Download manager + expiration | `DownloadManager.kt` | `DownloadManager.h/.cpp`, hooked in `BlackholeApp::HookDownloads` | ✅ Ported (Stage 2) — every download is redirected into `blackhole\` via `ICoreWebView2::add_DownloadStarting`, with collision-safe naming (`file (1).ext`) and WebView2's own download flyout suppressed |
| HTTP request tool (Postman-like) | `PostmanRequestDialog.kt`, `HttpMethod.kt` | `PostmanDialog.h/.cpp` | ✅ Ported |
| JS console | `JavaScriptConsole.kt` | `JsConsoleDialog.h/.cpp` | ✅ Ported |
| Data toolkit (Base64/hash) | `DataToolkit.kt` | `DataToolkitDialog.h/.cpp` | ✅ Ported (Base64 + MD5/SHA-1/SHA-256; hex encode/decode not yet added) |
| Per-tab JS toggle | `Settings.kt` (`js_toggle`) | "JS: On/Off" toolbar button → `ToggleJsForActiveTab()` | ✅ Ported (Stage 2) — flips `ICoreWebView2Settings::put_IsScriptEnabled` for the active tab only; button label updates on tab switch |
| Per-tab request log | `RequestLogEntry.kt` | `RequestLogDialog.h/.cpp`, "Request Log" toolbar button | ✅ Ported (Stage 2) — viewer dialog with a Refresh button; data capture (200-entry cap, newest-first, BLOCKED/allowed tags) was already in place from Stage 1 |
| SSL/TLS error handling | `onReceivedSslError` in `SecureWebView.kt` | N/A | ✅ Equivalent behavior for free — WebView2 always shows its own blocking certificate-error interstitial with no click-through API, so Strict/CTF can't accidentally affect it, same guarantee the Kotlin comment calls out. |
| No file:// access from web content | `SecureWebView.kt` | N/A | ⚠️ No direct WebView2 equivalent setting exists; left as a documented gap rather than a fake toggle. |
| SSH terminal | `SSHTerminal.kt`, `KnownHostsStore.kt` | — | ❌ Not ported (was already a placeholder in the Android app too — see upstream `README.md`'s "Known limitations") |
| Port scanner | `PortScanner.kt` | — | ❌ Not ported — see `PORTING_NOTES.md` |
| Hydra-lite credential tester | `HydraLite.kt` | — | ❌ Not ported — see `PORTING_NOTES.md` |
| Directory buster | `DirectoryBuster.kt` | — | ❌ Not ported — see `PORTING_NOTES.md` |
| Exploit shell (recon modules) | `ExploitShell.kt` | — | ❌ Not ported — see `PORTING_NOTES.md` |
| ~25 smaller dev-tool dialogs (JWT decoder, regex tester, WHOIS, TOTP, DNS lookup, TLS inspector, etc.) | various `*.kt` files | — | ❌ Not ported — low-risk, high-value next batch; see `PORTING_NOTES.md` |

## Settings file

`%APPDATA%\Blackhole\settings.ini` — plain `key=value` text, no external
JSON library dependency:

```ini
browserMode=0
securityMode=0
proxyEnabled=0
proxyScheme=socks5
proxyHost=127.0.0.1
proxyPort=9050
downloadRetentionHours=24
notificationsEnabled=1
```

## Ad blocker list format

`assets\adblock_hosts.txt` accepts either a bare hostname per line or a
hosts-file-style `0.0.0.0 example.com` line, so you can drop in
StevenBlack's unified hosts file unmodified — same note as the upstream
README's "Improving the ad blocker" section.

## Known rough edges (first pass, unbuilt)

Because this was written without the ability to compile against the real
WebView2 SDK headers, the most likely sources of build errors on your
first attempt are:
1. Exact `ICoreWebView2*` interface versions — the WebView2 SDK adds
   numbered interfaces (`ICoreWebView2_2`, `Settings3`, etc.) across
   releases; if a method isn't found on the base interface, it may need a
   `.As<ICoreWebView2_N>()` QueryInterface call I didn't add.
2. `Callback<>` lambda signatures — WRL's `Callback` template is picky
   about exact return-type/argument match with the event handler
   interface; a mismatch gives a verbose but fixable template error.
3. `wstring`/`LPCWSTR` lifetime around `CoTaskMemFree` — I tried to be
   consistent but a missed free (small leak, not a crash) is possible.

None of these are architectural problems — they're the normal shape of
first-build fixes for hand-written COM/WebView2 code. Send me the compiler
output and I'll patch directly.

## Next steps, roughly in priority order

1. Get it compiling (see `BUILD_WINDOWS.md`) — still not yet tested against
   a real WebView2 SDK as of Stage 2.
2. ~~Wire `DownloadManager` to `ICoreWebView2::add_DownloadStarting`~~ — done, Stage 2.
3. ~~Add the per-tab JS toggle button + per-tab request-log viewer~~ — done, Stage 2.
4. Port the ~25 smaller recon/dev-tool dialogs (JWT decoder, regex tester,
   WHOIS/DNS lookup, TOTP generator, TLS inspector, etc.) — these are all
   self-contained, low-risk utility dialogs similar in shape to
   `DataToolkitDialog.cpp`. (Stage 3+)
5. Decide on an approach for the four ByteBandit security tools (Hydra-lite,
   port scanner, directory buster, SSH terminal) — see `PORTING_NOTES.md`. (Stage 6)

## Stage 2 known rough edge

`ShowRequestLogForActiveTab()` captures a raw `Tab*` for its Refresh
callback. If you close that tab while the log viewer is still open and then
click Refresh, it reads a dangling pointer. Low-risk (single-threaded UI,
narrow window to hit it) but worth fixing in Stage 3 by having `CloseTab()`
close any log viewer associated with that tab first.
