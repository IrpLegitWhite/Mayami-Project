// src/vm/DllManager.h
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <windows.h>
#include "mayami/mayami_sdk.h"

namespace mayami {

    struct DllFuncEntry {
        std::string name;
        int arity;
        MymiNativeFn fn;
    };

    class DllManager {
    public:
        static DllManager& instance() {
            static DllManager inst;
            return inst;
        }

        void ensureLoaded() {
            if (m_loaded) return;
            m_loaded = true;

            // Определяем папку, где лежит сам .exe
            char exePath[MAX_PATH] = { 0 };
            GetModuleFileNameA(nullptr, exePath, MAX_PATH);
            std::string dir(exePath);
            size_t slash = dir.find_last_of("\\/");
            dir = (slash != std::string::npos) ? dir.substr(0, slash) : ".";

            // Настраиваем Host API для передачи в DLL
            m_api.hostCtx = this;
            m_api.register_fn = [](void* ctx, const char* name, int arity, MymiNativeFn fn) {
                auto* self = static_cast<DllManager*>(ctx);
                if (name && fn) {
                    self->m_funcs[name] = { name, arity, fn };
                }
                };
            m_api.alloc_str = [](void* ctx, const char* s) -> const char* {
                auto* self = static_cast<DllManager*>(ctx);
                if (!s) return "";
                self->m_strArena.emplace_back(s);
                return self->m_strArena.back().c_str();
                };
            m_api.new_array = nullptr;   // Пока не реализовано
            m_api.array_push = nullptr;

            // Ищем все .dll в папке <exe_dir>/libs/*.dll
            std::string libsDir = dir + "\\libs";
            std::string pattern = libsDir + "\\*.dll";
            WIN32_FIND_DATAA fd;
            HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
            if (hFind == INVALID_HANDLE_VALUE) return;

            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                std::string dllPath = libsDir + "\\" + fd.cFileName;
                HMODULE hMod = LoadLibraryA(dllPath.c_str());
                if (!hMod) continue;

                // ★ Перебираем возможные имена точек входа
                MymiModuleInitFn initFn = nullptr;
                const char* names[] = {
                    "mayami_module_init",
                    "mayami_module_init_fs",
                    "mayami_module_init_std",
                    nullptr
                };
                for (int i = 0; names[i]; ++i) {
                    initFn = (MymiModuleInitFn)GetProcAddress(hMod, names[i]);
                    if (initFn) break;
                }

                if (initFn) {
                    initFn(&m_api);
                    m_modules.push_back(hMod);
                }
                else {
                    FreeLibrary(hMod);
                }
            } while (FindNextFileA(hFind, &fd));

            FindClose(hFind);
        }

        const DllFuncEntry* find(const std::string& name) {
            ensureLoaded();
            auto it = m_funcs.find(name);
            return (it != m_funcs.end()) ? &it->second : nullptr;
        }

        void clearArena() {
            m_strArena.clear();
        }

        void* hostCtx() { return this; }

        ~DllManager() {
            for (HMODULE h : m_modules) {
                FreeLibrary(h);
            }
        }

    private:
        bool m_loaded = false;
        MymiHostAPI m_api{};
        std::vector<HMODULE> m_modules;
        std::unordered_map<std::string, DllFuncEntry> m_funcs;
        std::vector<std::string> m_strArena;
    };

} // namespace mayami