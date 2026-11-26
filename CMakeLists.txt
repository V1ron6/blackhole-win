cmake_minimum_required(VERSION 3.20)
project(Blackhole LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT WIN32)
    message(FATAL_ERROR "Blackhole (Windows port) only builds on Windows - it depends on WebView2, WinHTTP and BCrypt.")
endif()

# ---------------------------------------------------------------------------
# WebView2 SDK (NuGet package, restored automatically by Visual Studio, or
# fetched here via CMake's NuGet-less FetchContent from the nuget.org flat
# API if you build from the CLI). See docs/BUILD_WINDOWS.md for both paths.
# ---------------------------------------------------------------------------
if (DEFINED WEBVIEW2_DIR)
    set(WV2_DIR ${WEBVIEW2_DIR})
else()
    set(WV2_DIR "${CMAKE_BINARY_DIR}/webview2")
endif()

add_executable(Blackhole WIN32
    src/main.cpp
    src/BlackholeApp.cpp
    src/AdBlocker.cpp
    src/Settings.cpp
    src/DownloadManager.cpp
    src/PostmanDialog.cpp
    src/JsConsoleDialog.cpp
    src/DataToolkitDialog.cpp
    src/RequestLogDialog.cpp
    src/JsonFormatterDialog.cpp
    src/JwtDecoderDialog.cpp
    src/RegexTesterDialog.cpp
    src/HashIdentifierDialog.cpp
    src/TotpGeneratorDialog.cpp
    src/UserAgentSwitcherDialog.cpp
    src/HttpFetch.cpp
    src/DnsLookupDialog.cpp
    src/WhoisLookupDialog.cpp
    src/TlsInspectorDialog.cpp
    src/SecurityHeaderScannerDialog.cpp
    src/TechFingerprintDialog.cpp
    src/RobotsFetchDialog.cpp
    src/FaviconHashDialog.cpp
    src/AccessibilityCheckerDialog.cpp
    src/BookmarkletRunnerDialog.cpp
    src/CaseFileExportDialog.cpp
    src/CipherSolverDialog.cpp
    src/CookieInspectorDialog.cpp
    src/DiffViewerDialog.cpp
    src/FileInspectorDialog.cpp
    src/GraphQLIntrospectionDialog.cpp
    src/HarExportDialog.cpp
    src/RequestTimelineDialog.cpp
    src/ScreenshotCaptureDialog.cpp
    src/StorageInspectorDialog.cpp
    src/StringsExtractorDialog.cpp
    src/ViewportEmulatorDialog.cpp
    src/ExploitShellDialog.cpp
    src/ConcurrentScan.h
    src/WordlistLoader.cpp
    src/PortScannerDialog.cpp
    src/DirectoryBusterDialog.cpp
    src/HashCrackerDialog.cpp
    src/KnownHostsStore.cpp
    src/SshClient.cpp
    src/SSHTerminalDialog.cpp
    src/HydraLiteDialog.cpp
)

# ---------------------------------------------------------------------------
# libssh2 (OPTIONAL). Powers SSHTerminalDialog and HydraLiteDialog's SSH
# protocol module. Without it, both still build and run - they just report
# "SSH support was not compiled into this build" instead of connecting.
# Install via vcpkg: `vcpkg install libssh2:x64-windows`, then pass
# -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake to CMake
# (see docs/BUILD_WINDOWS.md, "Installing libssh2 (optional)").
# ---------------------------------------------------------------------------
find_package(Libssh2 CONFIG QUIET)
if (Libssh2_FOUND)
    target_compile_definitions(Blackhole PRIVATE BLACKHOLE_HAVE_LIBSSH2)
    target_link_libraries(Blackhole PRIVATE Libssh2::libssh2)
    message(STATUS "libssh2 found - SSH Terminal and Hydra-lite's SSH module will be functional.")
else()
    message(STATUS "libssh2 NOT found - SSH Terminal and Hydra-lite's SSH module will build but report "
                    "'not compiled into this build' at runtime. See docs/BUILD_WINDOWS.md to add it.")
endif()

target_include_directories(Blackhole PRIVATE
    src
    "${WV2_DIR}/build/native/include"
)

target_link_directories(Blackhole PRIVATE
    "${WV2_DIR}/build/native/${CMAKE_VS_PLATFORM_NAME}"
)

target_link_libraries(Blackhole PRIVATE
    WebView2Loader.dll
    winhttp.lib
    bcrypt.lib
    comctl32.lib
    shlwapi.lib
    shell32.lib
    ole32.lib
    crypt32.lib     # Base64 (DataToolkit, FaviconHash)
    dnsapi.lib      # DnsLookupDialog
    ws2_32.lib      # WhoisLookupDialog, DnsLookupDialog's InetNtopW
)

target_compile_definitions(Blackhole PRIVATE UNICODE _UNICODE NOMINMAX)
# NOMINMAX: several Stage 5 dialogs call std::min/std::max (e.g.
# DiffViewerDialog.cpp, FileInspectorDialog.cpp) - without this, windows.h's
# min()/max() macros would wreck those calls at preprocess time.

# Copy the WebView2Loader.dll and our assets next to the built exe so a
# plain double-click works without hunting for files.
add_custom_command(TARGET Blackhole POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${WV2_DIR}/build/native/${CMAKE_VS_PLATFORM_NAME}/WebView2Loader.dll"
        "$<TARGET_FILE_DIR:Blackhole>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:Blackhole>/assets"
)
