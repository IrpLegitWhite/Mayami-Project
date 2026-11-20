// libs/fs/fs.cpp
#include "mayami_sdk.h"
#include <fstream>
#include <sstream>
#include <windows.h>
#include <string>

static const MymiHostAPI* g_api = nullptr;

// fs_read(path)
static MymiVal fn_fs_read(int argc, const MymiVal* argv, void* ctx) {
    MymiVal res = { MYMI_NULL, 0, 0.0, 0, nullptr };
    if (argc < 1 || argv[0].type != MYMI_STRING) return res;

    std::ifstream file(argv[0].str, std::ios::binary);
    if (!file.is_open()) return res;

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    res.type = MYMI_STRING;
    res.str = g_api->alloc_str(ctx, content.c_str());
    return res;
}

// fs_write(path, text)
static MymiVal fn_fs_write(int argc, const MymiVal* argv, void* ctx) {
    MymiVal res = { MYMI_BOOL, 0, 0.0, 0, nullptr };
    if (argc < 2 || argv[0].type != MYMI_STRING || argv[1].type != MYMI_STRING) return res;

    std::ofstream file(argv[0].str, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return res;

    file << argv[1].str;
    res.b = 1;
    return res;
}

// fs_exists(path)
static MymiVal fn_fs_exists(int argc, const MymiVal* argv, void* ctx) {
    MymiVal res = { MYMI_BOOL, 0, 0.0, 0, nullptr };
    if (argc < 1 || argv[0].type != MYMI_STRING) return res;
    std::ifstream f(argv[0].str);
    res.b = f.good() ? 1 : 0;
    return res;
}

// fs_remove(path)
static MymiVal fn_fs_remove(int argc, const MymiVal* argv, void* ctx) {
    MymiVal res = { MYMI_BOOL, 0, 0.0, 0, nullptr };
    if (argc < 1 || argv[0].type != MYMI_STRING) return res;
    res.b = (std::remove(argv[0].str) == 0) ? 1 : 0;
    return res;
}

// ★ Точка входа для fs.dll
extern "C" MYMI_EXPORT void mayami_module_init_fs(const MymiHostAPI* api) {
    g_api = api;
    api->register_fn(api->hostCtx, "fs_read", 1, fn_fs_read);
    api->register_fn(api->hostCtx, "fs_write", 2, fn_fs_write);
    api->register_fn(api->hostCtx, "fs_exists", 1, fn_fs_exists);
    api->register_fn(api->hostCtx, "fs_remove", 1, fn_fs_remove);
}