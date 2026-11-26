#include "GraphQLIntrospectionDialog.h"
#include "HttpFetch.h"
#include <string>
#include <sstream>

namespace {
    constexpr int ID_URL = 2401, ID_QUERY = 2402, ID_RESULTS = 2403;
    const wchar_t* kClassName = L"BlackholeGraphQLIntrospection";

    struct Controls { HWND url, results; };
    Controls* GetControls(HWND h) { return reinterpret_cast<Controls*>(GetWindowLongPtr(h, GWLP_USERDATA)); }

    std::wstring GetText(HWND h) {
        int len = GetWindowTextLengthW(h);
        std::wstring s(len, L'\0');
        if (len > 0) GetWindowTextW(h, &s[0], len + 1);
        return s;
    }

    std::wstring FromUtf8(const std::vector<unsigned char>& bytes) {
        if (bytes.empty()) return {};
        int len = MultiByteToWideChar(CP_UTF8, 0, (const char*)bytes.data(), (int)bytes.size(), nullptr, 0);
        std::wstring w(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, (const char*)bytes.data(), (int)bytes.size(), &w[0], len);
        return w;
    }

    // A lightweight introspection query (just type + field names, not the
    // full spec query with descriptions/directives) - enough to map out a
    // schema without an overly large response.
    const wchar_t* kIntrospectionQuery =
        LR"({"query":"query IntrospectionQuery { __schema { queryType { name } mutationType { name } )"
        LR"(types { name kind fields { name } } } }"})";
}

void GraphQLIntrospectionDialog::RegisterClassOnce() {
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

void GraphQLIntrospectionDialog::ShowFor(HWND owner) {
    RegisterClassOnce();
    HWND hwnd = CreateWindowExW(0, kClassName, L"Blackhole - GraphQL Introspection",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 560, 460,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(hwnd, SW_SHOW);
}

void GraphQLIntrospectionDialog::RunQuery(HWND hwnd) {
    Controls* c = GetControls(hwnd);
    std::wstring url = GetText(c->url);
    if (url.empty()) return;

    SetWindowTextW(c->results, L"Querying...");
    HttpFetchResult r = HttpFetchPost(url, kIntrospectionQuery, L"application/json");
    if (!r.ok) {
        SetWindowTextW(c->results, (L"Request failed: " + r.error).c_str());
    } else {
        std::wstringstream out;
        out << L"HTTP " << r.statusCode << L"\r\n\r\n" << FromUtf8(r.body);
        SetWindowTextW(c->results, out.str().c_str());
    }
}

LRESULT CALLBACK GraphQLIntrospectionDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            Controls* c = new Controls();
            CreateWindowW(L"STATIC", L"GraphQL endpoint:", WS_CHILD | WS_VISIBLE, 10, 10, 150, 18,
                hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            c->url = CreateWindowW(L"EDIT", L"https://", WS_CHILD | WS_VISIBLE | WS_BORDER,
                10, 30, 400, 24, hwnd, (HMENU)(INT_PTR)ID_URL, GetModuleHandleW(nullptr), nullptr);
            CreateWindowW(L"BUTTON", L"Introspect", WS_CHILD | WS_VISIBLE, 420, 30, 110, 24,
                hwnd, (HMENU)(INT_PTR)ID_QUERY, GetModuleHandleW(nullptr), nullptr);
            c->results = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_HSCROLL, 10, 64, 520, 350,
                hwnd, (HMENU)(INT_PTR)ID_RESULTS, GetModuleHandleW(nullptr), nullptr);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HWND kids[] = { c->url, c->results };
            for (auto k : kids) SendMessageW(k, WM_SETFONT, (WPARAM)font, TRUE);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)c);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_QUERY && HIWORD(wParam) == BN_CLICKED) RunQuery(hwnd);
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
