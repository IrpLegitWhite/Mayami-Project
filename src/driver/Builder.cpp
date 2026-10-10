// src/driver/Builder.cpp
#include "Builder.h"
#include <cstdio>
#include <cstdlib>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#endif

namespace mayami {

    int Builder::build(const std::string& cppFile,
        const std::string& exeFile,
        const std::string& compilerPath) {
        std::string compiler = compilerPath;

        // Ищем компилятор
        if (compiler.empty()) {
#ifdef _WIN32
            // Пробуем g++ (MinGW)
            FILE* f = std::fopen("C:/msys64/mingw64/bin/g++.exe", "rb");
            if (f) { std::fclose(f); compiler = "C:/msys64/mingw64/bin/g++.exe"; }
            else {
                f = std::fopen("C:/MinGW/bin/g++.exe", "rb");
                if (f) { std::fclose(f); compiler = "C:/MinGW/bin/g++.exe"; }
                else compiler = "g++";   // в PATH
            }
#else
            compiler = "g++";
#endif
        }

        // Формируем команду
        std::string cmd = "\"" + compiler + "\" -std=c++17 -O2 -o \"" + exeFile + "\" \"" + cppFile + "\"";

#ifdef _WIN32
        // Windows: используем _popen
        FILE* pipe = _popen(cmd.c_str(), "r");
#else
        FILE* pipe = popen(cmd.c_str(), "r");
#endif
        if (!pipe) {
            std::fprintf(stderr, "Failed to run compiler\n");
            return 1;
        }

        // Читаем вывод компилятора
        char buf[512];
        while (std::fgets(buf, sizeof(buf), pipe)) {
            std::fprintf(stderr, "%s", buf);
        }

#ifdef _WIN32
        int status = _pclose(pipe);
#else
        int status = pclose(pipe);
#endif
        return status;
    }

} // namespace mayami