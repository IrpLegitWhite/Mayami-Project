// src/driver/Builder.h
#pragma once

#include <string>

namespace mayami {

    // ============================================================
    // Builder — компиляция C++ → exe
    // ============================================================
    class Builder {
    public:
        // Скомпилировать C++ файл в exe
        // compilerPath — путь к cl.exe или g++.exe
        // Возвращает 0 при успехе
        static int build(const std::string& cppFile,
            const std::string& exeFile,
            const std::string& compilerPath = "");
    };

} // namespace mayami