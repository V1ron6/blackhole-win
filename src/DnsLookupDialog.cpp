#include "DnsLookupDialog.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windns.h>
#include <string>
#include <sstream>

#pragma comment(lib, "dnsapi.lib")
#pragma comment(lib, "ws2_32.lib")

namespace {
    constexpr int ID_HOST = 1101, ID_LOOKUP = 1102, ID_RESULTS = 1103;
    const wchar_t* kClassName = L"BlackholeDnsLookup";

    struct Controls { HWND host, results; };
    Controls* GetControls(HWND h) { return reinterpret_cast<Controls*>(GetWindowLongPtr(h, GWLP_USERDATA)); }

    std::wstring GetText(HWND h) {
        int len = GetWindowTextLengthW(h);
        std::wstring s(len, L'\0');
        if (len > 0) GetWindowTextW(h, &s[0], len + 1);
        return s;
    }

    void QueryAndAppend(const std::wstring& host, WORD type, const wchar_t* label, std::wstringstream& out) {
        PDNS_RECORDW records = nullptr;
        DNS_STATUS status = DnsQuery_W(host.c_str(), type, DNS_QUERY_STANDARD, nullptr, &records, nullptr);
        if (status != 0 || !records) {
            out << label << L": (none)\r\n";
            return;
        }
        for (PDNS_RECORDW r = records; r != nullptr; r = r->pNext) {
            if (r->wType != type) continue;
            wchar_t buf[128];
            switch (type) {
                case DNS_TYPE_A:
                    if (InetNtopW(AF_INET, &r->Data.A.IpAddress, buf, 128))
                        out << label << L": " << buf << L"\r\n";
                    break;
                case DNS_TYPE_AAAA:
                    if (InetNtopW(AF_INET6, &r->Data.AAAA.Ip6Address, buf, 128))
                        out << label << L": " << buf << L"\r\n";
                    break;
                case DNS_TYPE_MX:
                    out << label << L": " << r->Data.MX.pNameExchange
                        << L" (preference " << r->Data.MX.wPreference << L")\r\n";
                    break;
                case DNS_TYPE_NS:
                    out << label << L": " << r->Data.NS.pNameHost << L"\r\n";
                    break;
                case DNS_TYPE_CNAME:
                    out << label << L": " << r->Data.CNAME.pNameHost << L"\r\n";
                    break;
                case DNS_TYPE_TEXT:
                    for (DWORD i = 0; i < r->Data.TXT.dwStringCount; ++i)
                        out << label << L": \"" << r->Data.TXT.pStringArray[i] << L"\"\r\n";
                    break;
            }
        }
        DnsRecordListFree(records, DnsFreeRecordList);
    }
}

void DnsLookupDialog::RegisterClassOnce() {
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

void DnsLookupDialog::ShowFor(HWND owner) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - DNS Lookup",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 520, 440,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(hwnd, SW_SHOW);
}

void DnsLookupDialog::RunLookup(HWND hwnd) {
    Controls* c = GetControls(hwnd);
    std::wstring host = GetText(c->host);
    if (host.empty()) return;

    SetWindowTextW(c->results, L"Looking up...");
    std::wstringstream out;
    QueryAndAppend(host, DNS_TYPE_A,     L"A",     out);
    QueryAndAppend(host, DNS_TYPE_AAAA,  L"AAAA",  out);
    QueryAndAppend(host, DNS_TYPE_CNAME, L"CNAME", out);
    QueryAndAppend(host, DNS_TYPE_MX,    L"MX",    out);
    QueryAndAppend(host, DNS_TYPE_NS,    L"NS",    out);
    QueryAndAppend(host, DNS_TYPE_TEXT,  L"TXT",   out);
    SetWindowTextW(c->results, out.str().c_str());
}

LRESULT CALLBACK DnsLookupDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            Controls* c = new Controls();
            CreateWindowW(L"STATIC", L"Hostname:", WS_CHILD | WS_VISIBLE, 10, 10, 100, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            c->host = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
                10, 30, 380, 24, hwnd, (HMENU)(INT_PTR)ID_HOST, GetModuleHandleW(nullptr), nullptr);
            CreateWindowW(L"BUTTON", L"Lookup", WS_CHILD | WS_VISIBLE, 400, 30, 90, 24,
                hwnd, (HMENU)(INT_PTR)ID_LOOKUP, GetModuleHandleW(nullptr), nullptr);
            c->results = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL, 10, 64, 480, 330,
                hwnd, (HMENU)(INT_PTR)ID_RESULTS, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HWND kids[] = { c->host, c->results };
            for (auto k : kids) SendMessageW(k, WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)c);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_LOOKUP && HIWORD(wParam) == BN_CLICKED) RunLookup(hwnd);
            return 0;
        case WM_DESTROY:
            delete GetControls(hwnd);
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
