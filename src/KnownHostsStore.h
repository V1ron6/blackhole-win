#pragma once
#include <string>

// Port of KnownHostsStore.kt. Stores pinned SSH host key fingerprints
// keyed by host:port, in a plain-text file at
// %APPDATA%\Blackhole\known_hosts.txt (same key=value format Settings.cpp
// uses) instead of Android SharedPreferences.
//
// Fingerprints here are SHA-256 over the raw host key bytes libssh2 hands
// back from libssh2_session_hostkey() - consistent across reconnects to
// the same host for pinning purposes, but NOT the same format
// `ssh-keygen -l` or a standard OpenSSH known_hosts file would show (those
// use the SSH wire encoding specifically, with some tools also including
// the key-type prefix in the hashed bytes). This is an internal pinning
// scheme for this app only, matching the original Kotlin version's own
// documented caveat about its fingerprint format.
class KnownHostsStore {
public:
    std::wstring GetFingerprint(const std::wstring& host, int port) const;
    void SaveFingerprint(const std::wstring& host, int port, const std::wstring& fingerprint);
    void Forget(const std::wstring& host, int port);

private:
    static std::wstring FilePath();
    static std::wstring Key(const std::wstring& host, int port);
};
