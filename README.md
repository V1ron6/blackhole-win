# Blackhole for Windows (10/11)

A native C++ port of the [Blackhole](../blackhole-main) Android browser —
a hardened, two-tab browser with CTF/pentest-lab modes. This port uses:

| Android piece                        | Windows equivalent                        |
|---------------------------------------|--------------------------------------------|
| `android.webkit.WebView`              | **WebView2** (Microsoft Edge/Chromium)     |
| Kotlin + AppCompat UI                 | **Win32 API** (plain windows/controls, no MFC/WinUI dependency) |
| `HttpURLConnection` (Postman tool)    | **WinHTTP**                                |
| `javax.crypto.MessageDigest`          | **Windows CNG** (`bcrypt.lib`)             |
| Gradle                                | **CMake**                                  |

It is **not** a wrapper or an emulator — it's a real native Windows
executable built against the same kind of Chromium engine Edge uses.

## What's implemented vs. what isn't

**Stage 1** covered **Basic, Intermediate, and Advance** modes, plus two of
ByteBandit's lower-risk utilities.

**Stage 2** (this build) finished the three things Stage 1 left half-done:
downloads now actually land in `blackhole\` with retention cleanup, the
per-tab JS toggle has a working toolbar button, and the per-tab request log
has a viewer dialog.

**Stage 3** (this build) adds six more Tools-menu dialogs: JSON formatter,
JWT decoder (decode-only, no signature check), regex tester, hash
identifier (pattern-match only, no cracking), TOTP generator, and
user-agent switcher.

**Stage 4** (this build) adds seven recon tools: DNS lookup, WHOIS lookup,
TLS inspector (cert/protocol/cipher details), security header scanner,
tech fingerprint, robots.txt fetch, and a Shodan-style favicon hash. A new
shared `HttpFetch` helper backs four of them so the WinHTTP boilerplate
isn't duplicated per-tool.

**Stage 5** (this build) adds the last 14 benign dev tools: accessibility
checker, bookmarklet runner, case file export, cipher solver, cookie
inspector, diff viewer, file inspector, GraphQL introspection, HAR export,
request timeline, screenshot capture, storage inspector, strings
extractor, and viewport emulator. **This closes out every non-flagged file
in the original app** — everything that's left is the handful of tools I
deliberately flagged for a separate decision.

**Stage 6** finishes the project: every remaining flagged tool is now
ported, faithfully to original scope, each gated behind ByteBandit mode:

- **Exploit Shell** — msfconsole-shaped shell, exactly three reconnaissance
  modules (HTTP/SSH/FTP banner grabs). No exploit/RCE modules, by the same
  permanent design constraint the original Kotlin file stated.
- **Port Scanner** — TCP connect-scan of the same 29 common ports.
- **Directory Buster** — path/file fuzzer, built-in or custom wordlist.
- **Hash Cracker** — dictionary + mutation-rule cracking for MD5/SHA-1/
  SHA-256/SHA-512/NTLM. bcrypt is listed but explicitly not functional
  (explained in-app rather than silently failing) — Windows has no native
  primitive for it, and hand-rolling Blowfish+bcrypt without a reference
  implementation to verify against was judged too risky for this pass.
- **SSH Terminal** and **Hydra-lite**'s SSH module — both need libssh2, an
  optional build dependency (see `docs/BUILD_WINDOWS.md`); without it they
  build fine and report plainly that SSH isn't available in that build.

**Every file from the original Android app is now ported** — `HashCracker`'s
bcrypt gap excepted, and `NotificationManager.kt`'s deliberate non-port
(superseded by `MessageBoxW` usage already throughout). See
[`docs/PORTING_NOTES.md`](docs/PORTING_NOTES.md) for the full file-by-file
status, and [`docs/DOCUMENTATION.md`](docs/DOCUMENTATION.md) for the
honest list of first-build risks across every stage.

The only work left on this project is the actual first build on real
Windows hardware.

## Quick start

1. Read [`docs/BUILD_WINDOWS.md`](docs/BUILD_WINDOWS.md) — you need Visual
   Studio 2022 (or the Build Tools) and the WebView2 SDK. **This project
   was written and documented on a Linux sandbox with no Windows machine
   or network access available, so it has not been compiled or run yet.**
   Expect to fix a handful of small build errors on your first build —
   paste them back to me and I'll patch the source.
2. Build with `./builder.sh` (Git Bash — see `docs/BUILD_WINDOWS.md` for the
   exact flags), plain CMake commands, or by opening the folder directly in
   Visual Studio, which understands `CMakeLists.txt` natively.
3. Run `Blackhole.exe`. Settings persist to
   `%APPDATA%\Blackhole\settings.ini`; downloads land in
   `%USERPROFILE%\Downloads\blackhole\`.

## Full documentation

See [`docs/DOCUMENTATION.md`](docs/DOCUMENTATION.md) for architecture,
every file's purpose, and how each Android feature maps to its Windows
counterpart.

## License

Same as upstream: CC0 1.0 Universal (public domain) — see `LICENSE` in the
original `blackhole-main` project.
