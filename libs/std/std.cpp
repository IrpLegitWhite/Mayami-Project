// libs/std/std.cpp
#include "mayami_sdk.h"
#include <cmath>
#include <string>
#include <sstream>
#include <algorithm>
#include <random>
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

static const MymiHostAPI* g_api = nullptr;
static std::mt19937_64 g_rng{ std::random_device{}() };

static MymiVal make_str(void* ctx, const std::string& s) {
    MymiVal v = { MYMI_STRING, 0, 0.0, 0, nullptr };
    v.str = g_api->alloc_str(ctx, s.c_str());
    return v;
}

static MymiVal make_int(int64_t i) {
    MymiVal v = { MYMI_INT, i, 0.0, 0, nullptr };
    return v;
}

static MymiVal make_float(double f) {
    MymiVal v = { MYMI_FLOAT, 0, f, 0, nullptr };
    return v;
}

static MymiVal make_bool(bool b) {
    MymiVal v = { MYMI_BOOL, 0, 0.0, b ? 1 : 0, nullptr };
    return v;
}

static MymiVal make_null() {
    return { MYMI_NULL, 0, 0.0, 0, nullptr };
}

// ============================================================
// МАТЕМАТИКА
// ============================================================
static MymiVal fn_math_sin(int argc, const MymiVal* argv, void* ctx) {
    if (argc < 1) return make_null();
    double x = (argv[0].type == MYMI_INT) ? (double)argv[0].i : argv[0].f;
    return make_float(std::sin(x));
}

static MymiVal fn_math_cos(int argc, const MymiVal* argv, void* ctx) {
    if (argc < 1) return make_null();
    double x = (argv[0].type == MYMI_INT) ? (double)argv[0].i : argv[0].f;
    return make_float(std::cos(x));
}

static MymiVal fn_math_tan(int argc, const MymiVal* argv, void* ctx) {
    if (argc < 1) return make_null();
    double x = (argv[0].type == MYMI_INT) ? (double)argv[0].i : argv[0].f;
    return make_float(std::tan(x));
}

static MymiVal fn_math_log(int argc, const MymiVal* argv, void* ctx) {
    if (argc < 1) return make_null();
    double x = (argv[0].type == MYMI_INT) ? (double)argv[0].i : argv[0].f;
    return make_float(std::log(x));
}

static MymiVal fn_math_log2(int argc, const MymiVal* argv, void* ctx) {
    if (argc < 1) return make_null();
    double x = (argv[0].type == MYMI_INT) ? (double)argv[0].i : argv[0].f;
    return make_float(std::log2(x));
}

static MymiVal fn_math_log10(int argc, const MymiVal* argv, void* ctx) {
    if (argc < 1) return make_null();
    double x = (argv[0].type == MYMI_INT) ? (double)argv[0].i : argv[0].f;
    return make_float(std::log10(x));
}

static MymiVal fn_math_exp(int argc, const MymiVal* argv, void* ctx) {
    if (argc < 1) return make_null();
    double x = (argv[0].type == MYMI_INT) ? (double)argv[0].i : argv[0].f;
    return make_float(std::exp(x));
}

static MymiVal fn_math_ceil(int argc, const MymiVal* argv, void* ctx) {
    if (argc < 1) return make_null();
    double x = (argv[0].type == MYMI_INT) ? (double)argv[0].i : argv[0].f;
    return make_int((int64_t)std::ceil(x));
}

static MymiVal fn_math_floor(int argc, const MymiVal* argv, void* ctx) {
    if (argc < 1) return make_null();
    double x = (argv[0].type == MYMI_INT) ? (double)argv[0].i : argv[0].f;
    return make_int((int64_t)std::floor(x));
}

static MymiVal fn_math_round(int argc, const MymiVal* argv, void* ctx) {
    if (argc < 1) return make_null();
    double x = (argv[0].type == MYMI_INT) ? (double)argv[0].i : argv[0].f;
    return make_int((int64_t)std::round(x));
}

static MymiVal fn_math_abs(int argc, const MymiVal* argv, void* ctx) {
    if (argc < 1) return make_null();
    if (argv[0].type == MYMI_INT) return make_int(std::abs(argv[0].i));
    return make_float(std::fabs(argv[0].f));
}

static MymiVal fn_math_pow(int argc, const MymiVal* argv, void* ctx) {
    if (argc < 2) return make_null();
    double a = (argv[0].type == MYMI_INT) ? (double)argv[0].i : argv[0].f;
    double b = (argv[1].type == MYMI_INT) ? (double)argv[1].i : argv[1].f;
    return make_float(std::pow(a, b));
}

static MymiVal fn_math_sqrt(int argc, const MymiVal* argv, void* ctx) {
    if (argc < 1) return make_null();
    double x = (argv[0].type == MYMI_INT) ? (double)argv[0].i : argv[0].f;
    return make_float(std::sqrt(x));
}

static MymiVal fn_math_min(int argc, const MymiVal* argv, void* ctx) {
    if (argc < 2) return make_null();
    double a = (argv[0].type == MYMI_INT) ? (double)argv[0].i : argv[0].f;
    double b = (argv[1].type == MYMI_INT) ? (double)argv[1].i : argv[1].f;
    return (argv[0].type == MYMI_INT && argv[1].type == MYMI_INT) ? make_int((int64_t)std::min((int64_t)a, (int64_t)b)) : make_float(std::min(a, b));
}

static MymiVal fn_math_max(int argc, const MymiVal* argv, void* ctx) {
    if (argc < 2) return make_null();
    double a = (argv[0].type == MYMI_INT) ? (double)argv[0].i : argv[0].f;
    double b = (argv[1].type == MYMI_INT) ? (double)argv[1].i : argv[1].f;
    return (argv[0].type == MYMI_INT && argv[1].type == MYMI_INT) ? make_int((int64_t)std::max((int64_t)a, (int64_t)b)) : make_float(std::max(a, b));
}

static MymiVal fn_math_pi(int argc, const MymiVal* argv, void* ctx) {
    return make_float(3.14159265358979323846);
}

static MymiVal fn_math_e(int argc, const MymiVal* argv, void* ctx) {
    return make_float(2.71828182845904523536);
}

static MymiVal fn_math_random(int argc, const MymiVal* argv, void* ctx) {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return make_float(dist(g_rng));
}

static MymiVal fn_math_randint(int argc, const MymiVal* argv, void* ctx) {
    if (argc < 2) return make_null();
    int64_t a = argv[0].i;
    int64_t b = argv[1].i;
    if (a > b) std::swap(a, b);
    std::uniform_int_distribution<int64_t> dist(a, b);
    return make_int(dist(g_rng));
}

// ============================================================
// СТРОКИ
// ============================================================
static MymiVal fn_str_len(int argc, const MymiVal* argv, void* ctx) {
    if (argc < 1 || argv[0].type != MYMI_STRING) return make_null();
    return make_int((int64_t)strlen(argv[0].str));
}

static MymiVal fn_str_upper(int argc, const MymiVal* argv, void* ctx) {
    if (argc < 1 || argv[0].type != MYMI_STRING) return make_null();
    std::string s = argv[0].str;
    for (auto& c : s) c = (char)std::toupper((unsigned char)c);
    return make_str(ctx, s);
}

static MymiVal fn_str_lower(int argc, const MymiVal* argv, void* ctx) {
    if (argc < 1 || argv[0].type != MYMI_STRING) return make_null();
    std::string s = argv[0].str;
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return make_str(ctx, s);
}

static MymiVal fn_str_contains(int argc, const MymiVal* argv, void* ctx) {
    if (argc < 2 || argv[0].type != MYMI_STRING || argv[1].type != MYMI_STRING) return make_null();
    return make_bool(std::string(argv[0].str).find(argv[1].str) != std::string::npos);
}

static MymiVal fn_str_replace(int argc, const MymiVal* argv, void* ctx) {
    if (argc < 3 || argv[0].type != MYMI_STRING || argv[1].type != MYMI_STRING || argv[2].type != MYMI_STRING) return make_null();
    std::string s = argv[0].str;
    std::string from = argv[1].str;
    std::string to = argv[2].str;
    if (!from.empty()) {
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.length(), to);
            pos += to.length();
        }
    }
    return make_str(ctx, s);
}

static MymiVal fn_str_substr(int argc, const MymiVal* argv, void* ctx) {
    if (argc < 3 || argv[0].type != MYMI_STRING) return make_null();
    std::string s = argv[0].str;
    int64_t start = argv[1].i;
    int64_t len = argv[2].i;
    if (start < 0) start = 0;
    if (start >= (int64_t)s.length()) return make_str(ctx, "");
    if (start + len > (int64_t)s.length()) len = (int64_t)s.length() - start;
    return make_str(ctx, s.substr(start, len));
}

static MymiVal fn_str_trim(int argc, const MymiVal* argv, void* ctx) {
    if (argc < 1 || argv[0].type != MYMI_STRING) return make_null();
    std::string s = argv[0].str;
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    if (start == std::string::npos) return make_str(ctx, "");
    return make_str(ctx, s.substr(start, end - start + 1));
}

static MymiVal fn_str_split(int argc, const MymiVal* argv, void* ctx) {
    if (argc < 2 || argv[0].type != MYMI_STRING || argv[1].type != MYMI_STRING) return make_null();
    std::string s = argv[0].str;
    std::string delim = argv[1].str;
    if (delim.empty()) return make_str(ctx, s);

    std::string result;
    size_t pos = 0, found;
    bool first = true;
    while ((found = s.find(delim, pos)) != std::string::npos) {
        if (!first) result += "|";
        result += s.substr(pos, found - pos);
        first = false;
        pos = found + delim.size();
    }
    if (!first) result += "|";
    result += s.substr(pos);
    return make_str(ctx, result);
}

// ============================================================
// HTML
// ============================================================
static MymiVal fn_html_extract_tag(int argc, const MymiVal* argv, void* ctx) {
    if (argc < 2 || argv[0].type != MYMI_STRING || argv[1].type != MYMI_STRING) return make_null();
    std::string html = argv[0].str;
    std::string tag = argv[1].str;
    std::string openTag = "<" + tag + ">";
    std::string closeTag = "</" + tag + ">";
    size_t start = html.find(openTag);
    if (start == std::string::npos) return make_str(ctx, "");
    start += openTag.length();
    size_t end = html.find(closeTag, start);
    if (end == std::string::npos) return make_str(ctx, "");
    return make_str(ctx, html.substr(start, end - start));
}

static MymiVal fn_html_strip_tags(int argc, const MymiVal* argv, void* ctx) {
    if (argc < 1 || argv[0].type != MYMI_STRING) return make_null();
    std::string html = argv[0].str;
    std::string result;
    bool inTag = false;
    for (char c : html) {
        if (c == '<') inTag = true;
        else if (c == '>') inTag = false;
        else if (!inTag) result += c;
    }
    return make_str(ctx, result);
}

// ============================================================
// СЕТЬ (WinHTTP)
// ============================================================
static MymiVal fn_net_get(int argc, const MymiVal* argv, void* ctx) {
    if (argc < 1 || argv[0].type != MYMI_STRING) return make_null();

    std::string url = argv[0].str;
    std::string host, path;
    bool isHttps = false;

    if (url.find("http://") == 0) {
        size_t slash = url.find('/', 7);
        host = url.substr(7, slash - 7);
        path = (slash != std::string::npos) ? url.substr(slash) : "/";
    }
    else if (url.find("https://") == 0) {
        isHttps = true;
        size_t slash = url.find('/', 8);
        host = url.substr(8, slash - 8);
        path = (slash != std::string::npos) ? url.substr(slash) : "/";
    }
    else {
        return make_str(ctx, "");
    }

    std::wstring whost(host.begin(), host.end());
    std::wstring wpath(path.begin(), path.end());

    HINTERNET hSession = WinHttpOpen(L"Mayami/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return make_str(ctx, "");

    INTERNET_PORT port = isHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    HINTERNET hConnect = WinHttpConnect(hSession, whost.c_str(), port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return make_str(ctx, ""); }

    DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wpath.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return make_str(ctx, ""); }

    BOOL bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (bResults) bResults = WinHttpReceiveResponse(hRequest, nullptr);

    std::string response;
    if (bResults) {
        DWORD dwSize = 0;
        do {
            DWORD dwDownloaded = 0;
            WinHttpQueryDataAvailable(hRequest, &dwSize);
            if (!dwSize) break;
            char* buffer = new char[dwSize + 1];
            ZeroMemory(buffer, dwSize + 1);
            if (!WinHttpReadData(hRequest, (LPVOID)buffer, dwSize, &dwDownloaded)) {
                delete[] buffer;
                break;
            }
            response.append(buffer, dwDownloaded);
            delete[] buffer;
        } while (dwSize > 0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return make_str(ctx, response);
}

// ============================================================
// РЕГИСТРАЦИЯ ФУНКЦИЙ
// ============================================================
extern "C" MYMI_EXPORT void mayami_module_init_std(const MymiHostAPI* api) {
    g_api = api;

    // Математика
    api->register_fn(api->hostCtx, "math_sin", 1, fn_math_sin);
    api->register_fn(api->hostCtx, "math_cos", 1, fn_math_cos);
    api->register_fn(api->hostCtx, "math_tan", 1, fn_math_tan);
    api->register_fn(api->hostCtx, "math_log", 1, fn_math_log);
    api->register_fn(api->hostCtx, "math_log2", 1, fn_math_log2);
    api->register_fn(api->hostCtx, "math_log10", 1, fn_math_log10);
    api->register_fn(api->hostCtx, "math_exp", 1, fn_math_exp);
    api->register_fn(api->hostCtx, "math_ceil", 1, fn_math_ceil);
    api->register_fn(api->hostCtx, "math_floor", 1, fn_math_floor);
    api->register_fn(api->hostCtx, "math_round", 1, fn_math_round);
    api->register_fn(api->hostCtx, "math_abs", 1, fn_math_abs);
    api->register_fn(api->hostCtx, "math_pow", 2, fn_math_pow);
    api->register_fn(api->hostCtx, "math_sqrt", 1, fn_math_sqrt);
    api->register_fn(api->hostCtx, "math_min", 2, fn_math_min);
    api->register_fn(api->hostCtx, "math_max", 2, fn_math_max);
    api->register_fn(api->hostCtx, "math_pi", 0, fn_math_pi);
    api->register_fn(api->hostCtx, "math_e", 0, fn_math_e);
    api->register_fn(api->hostCtx, "math_random", 0, fn_math_random);
    api->register_fn(api->hostCtx, "math_randint", 2, fn_math_randint);

    // Строки
    api->register_fn(api->hostCtx, "str_len", 1, fn_str_len);
    api->register_fn(api->hostCtx, "str_upper", 1, fn_str_upper);
    api->register_fn(api->hostCtx, "str_lower", 1, fn_str_lower);
    api->register_fn(api->hostCtx, "str_contains", 2, fn_str_contains);
    api->register_fn(api->hostCtx, "str_replace", 3, fn_str_replace);
    api->register_fn(api->hostCtx, "str_substr", 3, fn_str_substr);
    api->register_fn(api->hostCtx, "str_trim", 1, fn_str_trim);
    api->register_fn(api->hostCtx, "str_split", 2, fn_str_split);

    // HTML
    api->register_fn(api->hostCtx, "html_extract_tag", 2, fn_html_extract_tag);
    api->register_fn(api->hostCtx, "html_strip_tags", 1, fn_html_strip_tags);

    // Сеть
    api->register_fn(api->hostCtx, "net_get", 1, fn_net_get);
}