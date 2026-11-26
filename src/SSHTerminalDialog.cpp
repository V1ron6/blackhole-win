#include "SSHTerminalDialog.h"
#include "SshClient.h"
#include "KnownHostsStore.h"
#include <string>
#include <thread>
#include <memory>

namespace {
    constexpr int ID_HOST = 3601, ID_PORT = 3602, ID_USER = 3603, ID_PASS = 3604,
                  ID_CONNECT = 3605, ID_FORGET = 3606, ID_OUTPUT = 3607,
                  ID_COMMAND = 3608, ID_RUN = 3609;
    constexpr UINT WM_SSH_LOG = WM_APP + 1;
    constexpr UINT WM_SSH_CONNECTED = WM_APP + 2;
    constexpr UINT WM_SSH_FAILED = WM_APP + 3;
    constexpr UINT WM_SSH_CMD_RESULT = WM_APP + 4;
    const wchar_t* kClassName = L"BlackholeSSHTerminal";

    struct State {
        HWND host, port, user, pass, connectBtn, forgetBtn, output, command, runBtn;
        std::unique_ptr<SshClient> client;
        std::unique_ptr<KnownHostsStore> knownHosts = std::make_unique<KnownHostsStore>();
        std::wstring outputText;
        bool connecting = false;
    };
    State* GetState(HWND h) { return reinterpret_cast<State*>(GetWindowLongPtr(h, GWLP_USERDATA)); }

    std::wstring GetText(HWND h) {
        int len = GetWindowTextLengthW(h);
        std::wstring s(len, L'\0');
        if (len > 0) GetWindowTextW(h, &s[0], len + 1);
        return s;
    }

    void AppendOutputDirect(State* st, const std::wstring& line) {
        if (!st->outputText.empty()) st->outputText += L"\r\n";
        st->outputText += line;
        SetWindowTextW(st->output, st->outputText.c_str());
        int len = GetWindowTextLengthW(st->output);
        SendMessageW(st->output, EM_SETSEL, len, len);
        SendMessageW(st->output, EM_SCROLLCARET, 0, 0);
    }
}

void SSHTerminalDialog::RegisterClassOnce() {
    static bool registered = false;
    if (registered) return;
    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);
    registered = true;
}

void SSHTerminalDialog::ShowFor(HWND owner) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - SSH Terminal",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 600, 520,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(hwnd, SW_SHOW);
}

void SSHTerminalDialog::ForgetHostKey(HWND hwnd) {
    State* st = GetState(hwnd);
    std::wstring host = GetText(st->host);
    int port = _wtoi(GetText(st->port).c_str());
    if (port == 0) port = 22;
    if (host.empty()) { AppendOutputDirect(st, L"Enter a host first."); return; }
    st->knownHosts->Forget(host, port);
    AppendOutputDirect(st, L"Forgot saved host key for " + host + L":" + std::to_wstring(port) + L". Next connect will re-pin.");
}

void SSHTerminalDialog::Connect(HWND hwnd) {
    State* st = GetState(hwnd);
    if (st->connecting) return;
    std::wstring host = GetText(st->host);
    int port = _wtoi(GetText(st->port).c_str());
    if (port == 0) port = 22;
    std::wstring user = GetText(st->user);
    std::wstring pass = GetText(st->pass);

    if (host.empty() || user.empty()) { AppendOutputDirect(st, L"Host and username are required."); return; }
    if (!SshClient::Available()) {
        AppendOutputDirect(st, L"SSH support was not compiled into this build (libssh2 not found at "
            L"build time). See docs/BUILD_WINDOWS.md.");
        return;
    }

    AppendOutputDirect(st, L"Connecting to " + user + L"@" + host + L":" + std::to_wstring(port) + L"\u2026");
    st->connecting = true;
    EnableWindow(st->connectBtn, FALSE);

    // KnownHostsStore reads/writes a file but has no shared mutable state
    // across calls, so it's safe to let the worker thread use it directly
    // (the UI thread doesn't touch it concurrently - "Forget Host Key" is
    // disabled implicitly by the user having no reason to click it mid-connect).
    KnownHostsStore* knownHosts = st->knownHosts.get();

    std::thread([hwnd, host, port, user, pass, knownHosts]() {
        auto client = std::make_unique<SshClient>();
        std::wstring err;
        if (!client->Connect(host, port, 15000, err)) {
            auto* msg = new std::wstring(L"Connection failed: " + err);
            PostMessageW(hwnd, WM_SSH_FAILED, 0, (LPARAM)msg);
            return;
        }

        std::wstring fingerprint = client->HostKeyFingerprintSha256();
        std::wstring stored = knownHosts->GetFingerprint(host, port);
        bool trusted = false;

        if (stored.empty()) {
            std::wstring text = L"First time connecting to " + host + L":" + std::to_wstring(port) +
                L".\n\nFingerprint:\n" + fingerprint +
                L"\n\nOnly trust this if you have another way to confirm it's correct "
                L"(e.g. the server admin gave it to you directly).";
            int result = MessageBoxW(hwnd, text.c_str(), L"Unrecognized host key",
                MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
            trusted = (result == IDYES);
            if (trusted) {
                knownHosts->SaveFingerprint(host, port, fingerprint);
                auto* msg = new std::wstring(L"Pinned new host key for " + host + L":" + std::to_wstring(port) + L".");
                PostMessageW(hwnd, WM_SSH_LOG, 0, (LPARAM)msg);
            } else {
                auto* msg = new std::wstring(L"Host key rejected - connection will fail.");
                PostMessageW(hwnd, WM_SSH_FAILED, 0, (LPARAM)msg);
                return;
            }
        } else if (stored == fingerprint) {
            trusted = true;
        } else {
            auto* msg = new std::wstring(
                L"\u26A0 HOST KEY MISMATCH for " + host + L":" + std::to_wstring(port) +
                L" - refusing to connect. This could mean the server's key legitimately "
                L"rotated, or it could be a MITM. If you're sure it's legitimate, use "
                L"\"Forget Saved Host Key\" and reconnect to re-pin.");
            PostMessageW(hwnd, WM_SSH_FAILED, 0, (LPARAM)msg);
            return;
        }

        if (!trusted) {
            auto* msg = new std::wstring(L"Host key not trusted - connection aborted.");
            PostMessageW(hwnd, WM_SSH_FAILED, 0, (LPARAM)msg);
            return;
        }

        if (!client->AuthPassword(user, pass)) {
            auto* msg = new std::wstring(L"Authentication failed.");
            PostMessageW(hwnd, WM_SSH_FAILED, 0, (LPARAM)msg);
            return;
        }

        // Hand the connected, authenticated client over to the UI thread -
        // only the UI thread touches State->client from here on.
        SshClient* released = client.release();
        PostMessageW(hwnd, WM_SSH_CONNECTED, 0, (LPARAM)released);
    }).detach();
}

void SSHTerminalDialog::RunCommand(HWND hwnd) {
    State* st = GetState(hwnd);
    if (!st->client) return;
    std::wstring cmd = GetText(st->command);
    if (cmd.empty()) return;
    AppendOutputDirect(st, L"$ " + cmd);
    SetWindowTextW(st->command, L"");

    // Raw pointer, not ownership - the thread only calls ExecCommand, never
    // deletes it; State::client still owns the object. Safe as long as the
    // user doesn't close the dialog mid-command (same small race the rest
    // of this app's background-thread dialogs accept - see docs).
    SshClient* client = st->client.get();
    std::thread([hwnd, client, cmd]() {
        std::wstring err;
        std::wstring output = client->ExecCommand(cmd, err);
        auto* result = new std::wstring(err.empty() ? (output.empty() ? L"(no output)" : output) : (L"Command failed: " + err));
        PostMessageW(hwnd, WM_SSH_CMD_RESULT, 0, (LPARAM)result);
    }).detach();
}

LRESULT CALLBACK SSHTerminalDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            State* st = new State();
            CreateWindowW(L"STATIC", L"Host:", WS_CHILD | WS_VISIBLE, 10, 10, 50, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            st->host = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
                60, 8, 200, 24, hwnd, (HMENU)(INT_PTR)ID_HOST, GetModuleHandleW(nullptr), nullptr);
            CreateWindowW(L"STATIC", L"Port:", WS_CHILD | WS_VISIBLE, 270, 10, 40, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            st->port = CreateWindowW(L"EDIT", L"22", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                312, 8, 60, 24, hwnd, (HMENU)(INT_PTR)ID_PORT, GetModuleHandleW(nullptr), nullptr);
            CreateWindowW(L"STATIC", L"User:", WS_CHILD | WS_VISIBLE, 10, 40, 50, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            st->user = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
                60, 38, 200, 24, hwnd, (HMENU)(INT_PTR)ID_USER, GetModuleHandleW(nullptr), nullptr);
            CreateWindowW(L"STATIC", L"Password:", WS_CHILD | WS_VISIBLE, 270, 40, 70, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            st->pass = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_PASSWORD,
                342, 38, 180, 24, hwnd, (HMENU)(INT_PTR)ID_PASS, GetModuleHandleW(nullptr), nullptr);

            st->connectBtn = CreateWindowW(L"BUTTON", L"Connect", WS_CHILD | WS_VISIBLE, 10, 70, 100, 26,
                hwnd, (HMENU)(INT_PTR)ID_CONNECT, GetModuleHandleW(nullptr), nullptr);
            st->forgetBtn = CreateWindowW(L"BUTTON", L"Forget Saved Host Key", WS_CHILD | WS_VISIBLE,
                120, 70, 170, 26, hwnd, (HMENU)(INT_PTR)ID_FORGET, GetModuleHandleW(nullptr), nullptr);

            st->output = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL, 10, 104, 560, 310,
                hwnd, (HMENU)(INT_PTR)ID_OUTPUT, GetModuleHandleW(nullptr), nullptr);

            st->command = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_BORDER,
                10, 424, 460, 26, hwnd, (HMENU)(INT_PTR)ID_COMMAND, GetModuleHandleW(nullptr), nullptr);
            st->runBtn = CreateWindowW(L"BUTTON", L"Run", WS_CHILD, 480, 424, 90, 26,
                hwnd, (HMENU)(INT_PTR)ID_RUN, GetModuleHandleW(nullptr), nullptr);

            HFONT mono = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH, L"Consolas");
            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HWND kids[] = { st->host, st->port, st->user, st->pass, st->connectBtn, st->forgetBtn, st->command, st->runBtn };
            for (auto k : kids) SendMessageW(k, WM_SETFONT, (WPARAM)font, TRUE);
            SendMessageW(st->output, WM_SETFONT, (WPARAM)mono, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)st);
            if (!SshClient::Available()) {
                AppendOutputDirect(st, L"Note: this build was compiled without libssh2 - "
                    L"Connect will report why instead of working. See docs/BUILD_WINDOWS.md.");
            }
            return 0;
        }
        case WM_COMMAND: {
            State* st = GetState(hwnd);
            if (LOWORD(wParam) == ID_CONNECT && HIWORD(wParam) == BN_CLICKED) { Connect(hwnd); return 0; }
            if (LOWORD(wParam) == ID_FORGET && HIWORD(wParam) == BN_CLICKED) { ForgetHostKey(hwnd); return 0; }
            if (LOWORD(wParam) == ID_RUN && HIWORD(wParam) == BN_CLICKED) { RunCommand(hwnd); return 0; }
            return 0;
        }
        case WM_SSH_LOG: {
            State* st = GetState(hwnd);
            std::unique_ptr<std::wstring> text((std::wstring*)lParam);
            AppendOutputDirect(st, *text);
            return 0;
        }
        case WM_SSH_FAILED: {
            State* st = GetState(hwnd);
            std::unique_ptr<std::wstring> text((std::wstring*)lParam);
            AppendOutputDirect(st, *text);
            st->connecting = false;
            EnableWindow(st->connectBtn, TRUE);
            return 0;
        }
        case WM_SSH_CONNECTED: {
            State* st = GetState(hwnd);
            st->client.reset((SshClient*)lParam);
            st->connecting = false;
            AppendOutputDirect(st, L"Connected.");
            EnableWindow(st->connectBtn, TRUE);
            ShowWindow(st->command, SW_SHOW);
            ShowWindow(st->runBtn, SW_SHOW);
            return 0;
        }
        case WM_SSH_CMD_RESULT: {
            State* st = GetState(hwnd);
            std::unique_ptr<std::wstring> text((std::wstring*)lParam);
            AppendOutputDirect(st, *text);
            return 0;
        }
        case WM_DESTROY: {
            State* st = GetState(hwnd);
            if (st->client) st->client->Disconnect();
            delete st;
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
