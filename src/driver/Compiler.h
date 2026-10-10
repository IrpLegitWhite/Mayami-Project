// src/driver/Compiler.h
#pragma once

#include "../parser/AST.h"
#include <string>
#include <vector>
#include <memory>

namespace mayami {

    struct CompileResult {
        bool success = false;
        std::unique_ptr<Program> program;
        std::vector<std::string> errors;
        int tokenCount = 0;
    };

    class Compiler {
    public:
        static CompileResult compile(const std::string& source,
            const std::string& filename = "<input>");
        static CompileResult parseOnly(const std::string& source);
    };

} // namespace mayami