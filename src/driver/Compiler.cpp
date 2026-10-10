// src/driver/Compiler.cpp
#include "Compiler.h"
#include "../lexer/Lexer.h"
#include "../parser/Parser.h"
#include <cstdio>

namespace mayami {

    CompileResult Compiler::compile(const std::string& source,
        const std::string& filename) {
        (void)filename;
        return parseOnly(source);
    }

    CompileResult Compiler::parseOnly(const std::string& source) {
        CompileResult result;

        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        result.tokenCount = (int)tokens.size();

        if (lexer.hasErrors()) {
            for (const auto& e : lexer.errors())
                result.errors.push_back(e);
            return result;
        }

        Parser parser(tokens);
        auto program = parser.parse();

        if (parser.hasErrors()) {
            for (const auto& e : parser.errors())
                result.errors.push_back(e);
            return result;
        }

        result.success = true;
        result.program = std::move(program);
        return result;
    }

} // namespace mayami