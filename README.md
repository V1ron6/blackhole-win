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

Still not ported: ~19 smaller recon/dev-tool dialogs (WHOIS, DNS lookup,
TLS inspector, etc. — planned for Stage 4-5) and the four heavier
ByteBandit tools (SSH terminal, port scanner, Hydra-lite, directory buster
— planned for Stage 6, one at a time, only on request). See
[`docs/PORTING_NOTES.md`](docs/PORTING_NOTES.md) for the full file-by-file
status.

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
