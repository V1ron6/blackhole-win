# Building Blackhole on Windows 10/11

> **Honesty check first:** this source was written in a Linux sandbox with
> no network access and no Windows machine to test on. It follows the
> standard Microsoft WebView2 C++ sample patterns closely, but it has
> **not been compiled**. Budget 15–30 minutes for fixing small build
> errors (a missing include, an off-by-one in a signature) on your first
> attempt — that's normal for hand-written WebView2 C++ and not a sign
> anything is fundamentally wrong. Paste build errors back to me and I'll
> fix the source directly.

## 1. Prerequisites

- **Windows 10 (1809+) or Windows 11.**
- **Visual Studio 2022** (Community edition is fine) with the
  **"Desktop development with C++"** workload, which includes:
  - MSVC v143 toolset
  - Windows 10/11 SDK
  - CMake tools for Windows (or install CMake 3.20+ separately)
- **WebView2 Runtime.** Already installed on almost every Windows 10/11
  machine (it ships with Edge). If `Blackhole.exe` shows an error box about
  the WebView2 environment, grab the Evergreen Bootstrapper from
  Microsoft's WebView2 page and run it once.
- **WebView2 SDK** (the C++ headers + `WebView2Loader.dll`/`.lib`) — this
  is a NuGet package, **not** the runtime above. Two ways to get it:

### Option A — Visual Studio NuGet restore (easiest)

1. Open this folder in Visual Studio (`File → Open → Folder`, pick
   `blackhole-win`). VS detects `CMakeLists.txt` automatically.
2. Add a `packages.config` *or* just install the package via the NuGet
   Package Manager Console:
   ```
   Install-Package Microsoft.Web.WebView2 -Version 1.0.2792.45
   ```
3. Note the install path it prints (something like
   `...\packages\Microsoft.Web.WebView2.1.0.2792.45\`) and pass it to CMake:
   ```
   cmake -S . -B build -DWEBVIEW2_DIR="C:\path\to\packages\Microsoft.Web.WebView2.1.0.2792.45"
   ```

### Option B — vcpkg (command-line friendly)

```powershell
git clone https://github.com/microsoft/vcpkg
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg install webview2:x64-windows
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake
```
With vcpkg, `CMakeLists.txt`'s `WEBVIEW2_DIR` logic is bypassed — if you go
this route, tell me and I'll adjust `CMakeLists.txt` to use
`find_package(unofficial-webview2 CONFIG REQUIRED)` instead (vcpkg's port
exposes a different CMake target name than the raw NuGet layout this
`CMakeLists.txt` currently assumes).

## 2. Build

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DWEBVIEW2_DIR="<path from above>"
cmake --build build --config Release
```

The built `Blackhole.exe`, `WebView2Loader.dll`, and the `assets\` folder
(homepage + ad-block list) all land together in `build\Release\`. Keep them
together — the exe looks for `assets\homepage.html` and
`assets\adblock_hosts.txt` next to itself.

## 3. Common first-build issues

| Symptom | Likely cause |
|---|---|
| `Cannot open include file: 'WebView2.h'` | `WEBVIEW2_DIR` wasn't passed, or points at the wrong folder — it should contain `build\native\include\WebView2.h` |
| Linker error on `WebView2Loader.dll` | Same as above, or you built x86 instead of x64 (`-A x64`) |
| App launches but shows a blank window / WebView2 error box | WebView2 **Runtime** (not SDK) missing — see Prerequisites |
| `wWinMain` not found / subsystem mismatch | Make sure `add_executable(Blackhole WIN32 ...)` in `CMakeLists.txt` wasn't edited — the `WIN32` keyword is what tells the linker to use the GUI subsystem and look for `wWinMain` |

## 4. Making a single portable .exe (optional)

By default you get `Blackhole.exe` + `WebView2Loader.dll` + `assets\` as a
folder. If you want one file to hand to someone:

- Static-link the C runtime: add `set_property(TARGET Blackhole PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")` to
  `CMakeLists.txt` (I can add this for you on request).
- `WebView2Loader.dll` can be statically linked instead of loaded
  dynamically — swap `WebView2Loader.dll` for `WebView2LoaderStatic.lib` in
  `target_link_libraries` (also happy to wire this up on request).
- The `assets\` folder can't be merged into the .exe without adding a
  resource-embedding step (RCDATA resources + `ExtractToTempFolder()` on
  startup) — ask if you want this added.
