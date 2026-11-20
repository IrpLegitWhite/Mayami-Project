// src/lexer/Highlighter.h
#pragma once

#include <string>
#include <vector>

namespace mayami {

    enum class HighlightType {
        Default,
        Keyword,        // fn, let, if, ...
        Type,           // i32, f64, string, ...
        Constant,       // true, false
        Function,       // println, print, ...
        Identifier,     // переменные
        Number,
        String,
        Char,
        Comment,
        DocComment,
        Operator,
        Punctuation,
        Preprocessor,
    };

    struct HighlightSegment {
        HighlightType type;
        size_t start;
        size_t length;
    };

    class Highlighter {
    public:
        // Подсветка одной строки. inBlockComment — если строка начинается внутри /* */
        static std::vector<HighlightSegment>
            highlightLine(const std::string& line, bool& inBlockComment);
    };

} // namespace mayami