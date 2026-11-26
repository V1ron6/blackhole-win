#include "SshClient.h"

#ifdef BLACKHOLE_HAVE_LIBSSH2

#include <winsock2.h>
#include <ws2tcpip.h>
#include <libssh2.h>
#include <bcrypt.h>
#include <string>
#include <vector>
#include <sstream>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "bcrypt.lib")

namespace {
    std::string ToUtf8(const std::wstring& w) {
        if (w.empty()) return {};
        int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
        std::string s(len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], len, nullptr, nullptr);
        return s;
    }
    std::wstring FromUtf8(const std::string& s) {
        if (s.empty()) return {};
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
        std::wstring w(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], len);
        return w;
    }

    std::wstring Sha256FingerprintColon(const char* data, size_t len) {
        BCRYPT_ALG_HANDLE hAlg = nullptr;
        if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return L"";
        DWORD hashLen = 0, cbResult = 0;
        BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashLen, sizeof(hashLen), &cbResult, 0);
        std::vector<UCHAR> hash(hashLen);
        BCRYPT_HASH_HANDLE hHash = nullptr;
        BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
        BCryptHashData(hHash, (PUCHAR)data, (ULONG)len, 0);
        BCryptFinishHash(hHash, hash.data(), hashLen, 0);
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);

        std::wstringstream out;
        for (size_t i = 0; i < hash.size(); ++i) {
            wchar_t buf[3]; wsprintfW(buf, L"%02x", hash[i]);
            if (i) out << L":";
            out << buf;
        }
        return out.str();
    }
}

bool SshClient::Available() { return true; }

SshClient::SshClient() {
    static bool wsaInited = false;
    if (!wsaInited) { WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa); wsaInited = true; }
    libssh2_init(0);
}

SshClient::~SshClient() { Disconnect(); }

bool SshClient::Connect(const std::wstring& host, int port, int timeoutMs, std::wstring& err) {
    addrinfo hints{}; hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    std::string hostUtf8 = ToUtf8(host);
    if (getaddrinfo(hostUtf8.c_str(), std::to_string(port).c_str(), &hints, &res) != 0 || !res) {
        err = L"Could not resolve host: " + host;
        return false;
    }
    SOCKET sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock != INVALID_SOCKET) {
        DWORD timeout = (DWORD)timeoutMs;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
    }
    if (sock == INVALID_SOCKET || connect(sock, res->ai_addr, (int)res->ai_addrlen) != 0) {
        err = L"Could not connect to " + host + L":" + std::to_wstring(port);
        if (sock != INVALID_SOCKET) closesocket(sock);
        freeaddrinfo(res);
        return false;
    }
    freeaddrinfo(res);
    socket_ = (uintptr_t)sock;

    LIBSSH2_SESSION* session = libssh2_session_init();
    if (!session) { err = L"libssh2_session_init failed."; closesocket(sock); return false; }
    libssh2_session_set_blocking(session, 1);

    if (libssh2_session_handshake(session, sock) != 0) {
        err = L"SSH handshake failed.";
        libssh2_session_free(session);
        closesocket(sock);
        return false;
    }
    session_ = session;
    connected_ = true;
    return true;
}

std::wstring SshClient::HostKeyFingerprintSha256() const {
    if (!session_) return L"";
    size_t len = 0; int type = 0;
    const char* key = libssh2_session_hostkey((LIBSSH2_SESSION*)session_, &len, &type);
    if (!key || len == 0) return L"";
    return Sha256FingerprintColon(key, len);
}

bool SshClient::AuthPassword(const std::wstring& user, const std::wstring& pass) {
    if (!session_) return false;
    std::string userUtf8 = ToUtf8(user), passUtf8 = ToUtf8(pass);
    return libssh2_userauth_password((LIBSSH2_SESSION*)session_, userUtf8.c_str(), passUtf8.c_str()) == 0;
}

std::wstring SshClient::ExecCommand(const std::wstring& command, std::wstring& err) {
    if (!session_) { err = L"Not connected."; return L""; }
    LIBSSH2_SESSION* session = (LIBSSH2_SESSION*)session_;
    LIBSSH2_CHANNEL* channel = libssh2_channel_open_session(session);
    if (!channel) { err = L"Could not open channel."; return L""; }

    std::string cmdUtf8 = ToUtf8(command);
    if (libssh2_channel_exec(channel, cmdUtf8.c_str()) != 0) {
        err = L"Command execution failed.";
        libssh2_channel_free(channel);
        return L"";
    }

    std::string output;
    char buf[4096];
    ssize_t n;
    while ((n = libssh2_channel_read(channel, buf, sizeof(buf))) > 0) {
        output.append(buf, n);
    }
    libssh2_channel_close(channel);
    libssh2_channel_free(channel);
    return FromUtf8(output);
}

void SshClient::Disconnect() {
    if (session_) {
        libssh2_session_disconnect((LIBSSH2_SESSION*)session_, "Blackhole closing session");
        libssh2_session_free((LIBSSH2_SESSION*)session_);
        session_ = nullptr;
    }
    if (socket_) {
        closesocket((SOCKET)socket_);
        socket_ = 0;
    }
    connected_ = false;
}

#else // !BLACKHOLE_HAVE_LIBSSH2

bool SshClient::Available() { return false; }
SshClient::SshClient() = default;
SshClient::~SshClient() = default;
bool SshClient::Connect(const std::wstring&, int, int, std::wstring& err) {
    err = L"SSH support was not compiled into this build (libssh2 not found at build time - "
          L"see docs/BUILD_WINDOWS.md, \"Installing libssh2 (optional)\").";
    return false;
}
std::wstring SshClient::HostKeyFingerprintSha256() const { return L""; }
bool SshClient::AuthPassword(const std::wstring&, const std::wstring&) { return false; }
std::wstring SshClient::ExecCommand(const std::wstring&, std::wstring& err) {
    err = L"SSH support was not compiled into this build.";
    return L"";
}
void SshClient::Disconnect() {}

#endif
