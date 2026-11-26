#pragma once
#include <windows.h>

// Port of SSHTerminal.kt. Log into a server and run commands. Credentials
// live only in this dialog's State struct in memory and are never written
// to Settings or disk; the connection is torn down when the dialog closes
// - same guarantee the original Kotlin doc comment made.
//
// Host key verification is trust-on-first-use via KnownHostsStore, same
// flow as the original: first connection to a host:port prompts to accept
// and pin its SHA-256 fingerprint; later connections compare silently and
// refuse outright on a mismatch, with the same "this could be a legitimate
// rotation or a MITM" wording.
//
// Needs libssh2 at build time - see SshClient.h. If this build doesn't
// have it, Connect reports that plainly instead of the UI pretending the
// feature works.
class SSHTerminalDialog {
public:
    static void ShowFor(HWND owner);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
    static void Connect(HWND hwnd);
    static void RunCommand(HWND hwnd);
    static void ForgetHostKey(HWND hwnd);
};
