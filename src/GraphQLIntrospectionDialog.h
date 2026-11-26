#pragma once
#include <windows.h>

// Port of GraphQLIntrospection.kt. Sends the standard GraphQL introspection
// query (__schema { types { name } }) to a given endpoint - a query every
// spec-compliant GraphQL server answers unless introspection has been
// explicitly disabled; this is not an exploit, it's documented protocol
// behavior (same as visiting a REST API's OpenAPI/Swagger JSON).
class GraphQLIntrospectionDialog {
public:
    static void ShowFor(HWND owner);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClassOnce();
    static void RunQuery(HWND hwnd);
};
