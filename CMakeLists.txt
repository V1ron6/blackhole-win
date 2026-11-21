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
)

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
)

target_compile_definitions(Blackhole PRIVATE UNICODE _UNICODE)

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
