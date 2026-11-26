#pragma once
#include <string>

// Stage 6: thin wrapper around libssh2, shared by SSHTerminalDialog and
// HydraLiteDialog's SSH module - both need "connect, get host key
// fingerprint, auth with a password, optionally run one command."
//
// libssh2 is an OPTIONAL build dependency (see docs/BUILD_WINDOWS.md,
// "Installing libssh2 (optional)"). The real SSH protocol - key exchange,
// encryption, the wire format - is not something to hand-roll from scratch
// in a single pass without a way to test it against a real server; libssh2
// is a mature, narrowly-scoped C library built for exactly this (used by
// curl, git, and many other mainstream tools), the same category of
// external dependency this project already has in WebView2.
//
// If BLACKHOLE_HAVE_LIBSSH2 isn't defined at compile time (library not
// found by CMake), every method below returns a clear "not available in
// this build" failure instead of the feature silently doing nothing or the
// whole project failing to compile.
class SshClient {
public:
    SshClient();
    ~SshClient();

    static bool Available(); // true if this build was compiled with libssh2

    // Connects and completes the SSH handshake (but not auth). On success,
    // HostKeyFingerprintSha256() is available for host-key verification
    // before you decide whether to call AuthPassword.
    bool Connect(const std::wstring& host, int port, int timeoutMs, std::wstring& err);

    // SHA-256 over the raw host key bytes, formatted "aa:bb:cc:..." - same
    // format KnownHostsStore stores and SSHTerminal.kt originally computed
    // (over a different, Java-specific encoding - see KnownHostsStore.h's
    // header comment on why the two aren't directly comparable formats).
    std::wstring HostKeyFingerprintSha256() const;

    bool AuthPassword(const std::wstring& user, const std::wstring& pass);

    // Runs one command in a fresh channel and returns its stdout, or sets
    // err and returns an empty string on failure.
    std::wstring ExecCommand(const std::wstring& command, std::wstring& err);

    void Disconnect();

private:
    void* session_ = nullptr; // LIBSSH2_SESSION*, opaque here to keep this header libssh2-free
    uintptr_t socket_ = 0;    // SOCKET, stored as uintptr_t for the same reason
    bool connected_ = false;
};
