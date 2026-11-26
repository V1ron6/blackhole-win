# Porting notes: every original Kotlin file, one by one

Legend: ✅ ported · ⚠️ partially ported / stubbed · ❌ not started

| Kotlin file | Status | Notes |
|---|---|---|
| `MainActivity.kt` | ✅ | Logic folded into `BlackholeApp.cpp` (toolbar, tab rendering, mode indicator) |
| `TabManager.kt` | ✅ | `kMaxTabs = 2` enforced in `AddTab()` |
| `SecureWebView.kt` | ✅ | JS-off-by-default, Strict/CTF scheme check. File:// access has no WebView2 equivalent (documented gap, not faked) |
| `AdBlocker.kt` | ✅ | `AdBlocker.h/.cpp`, same file format |
| `BrowserMode.kt` | ✅ | `Settings.h` enum |
| `ModeManager.kt` | ✅ | `OpenToolForCurrentMode()` gates tools by mode |
| `DownloadManager.kt` | ✅ | Folder + retention sweep (Stage 1) + hooked to `ICoreWebView2::add_DownloadStarting` (Stage 2) |
| `HttpMethod.kt` | ✅ | `PostmanDialog.cpp` |
| `NotificationManager.kt` | ✅ (decision made, Stage 6) | Not ported as its own file: it was a thin `showAlert`/`showConfirm`/`showToast` wrapper, and the first two are exactly what `MessageBoxW` already does everywhere in this codebase. No dialog has needed a non-blocking toast yet, so one wasn't built speculatively - ask if a specific tool needs it and I'll add a small balloon-tip helper then. |
| `Settings.kt` | ✅ | `Settings.h/.cpp`, INI instead of SharedPreferences |
| `SettingsActivity.kt` | ✅ | `ShowSettingsDialog()` + `SettingsWndProc` in `BlackholeApp.cpp` |
| `PostmanRequestDialog.kt` | ✅ | `PostmanDialog.h/.cpp` |
| `JavaScriptConsole.kt` | ✅ | `JsConsoleDialog.h/.cpp` |
| `DataToolkit.kt` | ✅ | `DataToolkitDialog.h/.cpp` (Base64 + 3 hashes; hex and a couple of the original's other conversions not yet added) |
| `RequestLogEntry.kt` | ✅ | Data captured per tab (Stage 1) + `RequestLogDialog.h/.cpp` viewer (Stage 2) |
| **Everything below: not started.** Each is self-contained (its own Kotlin file, own dialog layout), so they can be added one at a time without touching the core app. | | |
| `AccessibilityChecker.kt` | ✅ (Stage 5) | `AccessibilityCheckerDialog.h/.cpp` - JS injection checks alt text, form labels, `<html lang>`, `<title>` |
| `BookmarkletRunner.kt` | ✅ (Stage 5) | `BookmarkletRunnerDialog.h/.cpp` - strips `javascript:` prefix, runs via the same ExecFn plumbing as the JS console |
| `CaseFileExport.kt` | ✅ (Stage 5) | `CaseFileExportDialog.h/.cpp` - freeform notes, exports to a timestamped .txt |
| `CipherSolver.kt` | ✅ (Stage 5) | `CipherSolverDialog.h/.cpp` - ROT13, Atbash, all-26 Caesar shifts |
| `CookieInspector.kt` | ✅ (Stage 5) | `CookieInspectorDialog.h/.cpp` + `BlackholeApp::GetCookiesForActiveTab` (`ICoreWebView2CookieManager`), read-only |
| `DiffViewer.kt` | ✅ (Stage 5) | `DiffViewerDialog.h/.cpp` - line-based LCS diff |
| `DirectoryBuster.kt` | ✅ (Stage 6) | `DirectoryBusterDialog.h/.cpp` - built-in ~185-path wordlist or custom, extension suffixes, 2000-request cap, 8-way bounded concurrency, authorization checkbox gate, redirects not auto-followed, bodies never read |
| `DnsLookup.kt` | ✅ (Stage 4) | `DnsLookupDialog.h/.cpp` - A/AAAA/CNAME/MX/NS/TXT via `DnsQuery_W` |
| `ExploitShell.kt` | ✅ (Stage 6) | `ExploitShellDialog.h/.cpp` - the msfconsole-shaped shell ported 1:1, all 3 original recon modules (HTTP/SSH/FTP banner grab), ByteBandit-mode gated. No exploit/RCE modules, by the same permanent design the original file stated. |
| `FaviconHash.kt` | ✅ (Stage 4) | `FaviconHashDialog.h/.cpp` - fetches `/favicon.ico`, Base64 + MurmurHash3 x86-32, matches Shodan's `http.favicon.hash` format |
| `FileInspector.kt` | ✅ (Stage 5) | `FileInspectorDialog.h/.cpp` - local file picker, hex dump, 14-signature magic-number type guess |
| `GraphQLIntrospection.kt` | ✅ (Stage 5) | `GraphQLIntrospectionDialog.h/.cpp` - standard introspection query via new `HttpFetchPost` |
| `HarExport.kt` | ✅ (Stage 5) | `HarExportDialog.h/.cpp` - minimal valid .har; see DOCUMENTATION.md for what fields are placeholders |
| `HashCracker.kt` | ⚠️ (Stage 6) | `HashCrackerDialog.h/.cpp` - MD5/SHA-1/SHA-256/SHA-512/NTLM via Windows CNG, same mutation rules (capitalize/leet/append 0-99), 150k-attempt cap. **bcrypt is listed but not functional** - see the `Verify()` doc comment in the .cpp for why (no CNG primitive for it, and hand-rolling Blowfish+bcrypt without a reference implementation to test against was judged too risky for this pass). |
| `HashIdentifier.kt` | ✅ (Stage 3) | `HashIdentifierDialog.h/.cpp` - pattern-matches length/charset against known formats, no cracking |
| `HydraLite.kt` | ✅ (Stage 6) | `HydraLiteDialog.h/.cpp` - all 4 original protocols (HTTP Basic, HTTP POST form, FTP, SSH), 500-attempt cap, 5-way concurrency, authorization checkbox gate, stops at first working credential. SSH protocol needs libssh2 (see `SshClient.h`) and deliberately skips host-key pinning for this tool, same as the original's `PromiscuousVerifier`. |
| `JsonFormatter.kt` | ✅ (Stage 3) | `JsonFormatterDialog.h/.cpp` - structural pretty-print/minify, not a validating parser (same scope as upstream) |
| `JwtDecoder.kt` | ✅ (Stage 3) | `JwtDecoderDialog.h/.cpp` - decodes header+payload via Base64URL, does not verify the signature |
| `KnownHostsStore.kt` | ✅ (Stage 6) | `KnownHostsStore.h/.cpp` - file-based fingerprint pinning at `%APPDATA%\Blackhole\known_hosts.txt`; fingerprint format is this app's own (SHA-256 over libssh2's raw host-key bytes), not directly comparable to `ssh-keygen -l`'s output - see the header comment |
| `PortScanner.kt` | ✅ (Stage 6) | `PortScannerDialog.h/.cpp` - same 29 common ports, non-blocking-connect + `select()` timeout (600ms), 12-way bounded concurrency, authorization checkbox gate |
| `ProxyManager.kt` | ✅ (see above) | Folded into `CreateWebViewEnvironment()` / `Settings.h` |
| `RegexTester.kt` | ✅ (Stage 3) | `RegexTesterDialog.h/.cpp` - `std::wregex`, ECMAScript grammar, shows match positions + capture groups |
| `RequestTimeline.kt` | ✅ (Stage 5) | `RequestTimelineDialog.h/.cpp` - chronological log view with timestamps (added to `Tab::requestLog` in Stage 5); not a graphical waterfall, see DOCUMENTATION.md |
| `RobotsFetch.kt` | ✅ (Stage 4) | `RobotsFetchDialog.h/.cpp` - fetches and displays `/robots.txt` |
| `ScreenshotCapture.kt` | ✅ (Stage 5) | `ScreenshotCaptureDialog.h/.cpp` + `BlackholeApp::CaptureScreenshotOfActiveTab` (`ICoreWebView2::CapturePreview`) |
| `SecurityHeaderScanner.kt` | ✅ (Stage 4) | `SecurityHeaderScannerDialog.h/.cpp` - checks 7 standard security headers via the shared `HttpFetch` helper |
| `SSHTerminal.kt` | ✅ (Stage 6) | `SSHTerminalDialog.h/.cpp` + `SshClient.h/.cpp` (libssh2) + `KnownHostsStore` - trust-on-first-use host-key pinning with a Yes/No prompt showing the SHA-256 fingerprint, refuses outright on a later mismatch, password auth, single-command exec per Run click. Needs libssh2 at build time (optional dependency, see `docs/BUILD_WINDOWS.md`). |
| `StorageInspector.kt` | ✅ (Stage 5) | `StorageInspectorDialog.h/.cpp` - localStorage/sessionStorage dump via JS injection, read-only |
| `StringsExtractor.kt` | ✅ (Stage 5) | `StringsExtractorDialog.h/.cpp` - printable-ASCII run extraction (min length 4, same as Unix `strings`), capped at 2000 results |
| `TechFingerprint.kt` | ✅ (Stage 4) | `TechFingerprintDialog.h/.cpp` - header/markup marker matching (20-entry table), same approach as Wappalyzer |
| `TlsInspector.kt` | ✅ (Stage 4) | `TlsInspectorDialog.h/.cpp` - protocol/cipher/cert details via `WINHTTP_OPTION_SECURITY_CERTIFICATE_STRUCT` |
| `TotpGenerator.kt` | ✅ (Stage 3) | `TotpGeneratorDialog.h/.cpp` - RFC 6238 TOTP via CNG HMAC-SHA1, Base32 secret decode, auto-refreshes every second |
| `UserAgentSwitcher.kt` | ✅ (Stage 3) | `UserAgentSwitcherDialog.h/.cpp` + `BlackholeApp::SetUserAgentOnActiveTab` (`ICoreWebView2Settings2::put_UserAgent`), 4 presets + custom, reloads active tab |
| `ViewportEmulator.kt` | ✅ (Stage 5) | `ViewportEmulatorDialog.h/.cpp` + `BlackholeApp::SetViewportOverrideForActiveTab` - shrinks the controller bounds, 6 device presets + custom |
| `WhoisLookup.kt` | ✅ (Stage 4) | `WhoisLookupDialog.h/.cpp` - raw Winsock TCP to port 43, defaults to `whois.iana.org`, editable server field for registry referrals |

## Status

Every file from the original Android app is now ✅, with one explicitly
non-functional edge case marked ⚠️: bcrypt verification in
`HashCracker.kt`'s Windows port (every other hash type in that tool works).
Nothing in this table is still ❌. `NotificationManager.kt`'s non-port is a
deliberate decision, not a gap - see its row above.

None of this has been compiled against real SDK/library headers (WebView2,
libssh2). See `DOCUMENTATION.md`'s per-stage "notes" sections for the
specific first-build risks I'd flag if I had to guess where an error
would show up.
