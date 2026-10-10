// src/lexer/Highlighter.cpp
#include "Highlighter.h"
#include <unordered_set>
#include <cctype>
#include <cstring>

namespace mayami {

    static const std::unordered_set<std::string> KEYWORDS = {
        "fn", "let", "mut", "if", "else", "while", "for", "loop",
        "break", "continue", "return", "struct", "enum", "trait",
        "impl", "match", "async", "await", "pub", "use", "mod",
        "const", "static", "in", "as", "self", "Self", "where",
        "crate", "super", "dyn", "ref", "move", "unsafe", "extern"
    };

    static const std::unordered_set<std::string> TYPES = {
        "i8", "i16", "i32", "i64", "i128",
        "u8", "u16", "u32", "u64", "u128",
        "f32", "f64",
        "bool", "string", "char", "void",
        "str", "usize", "isize"
    };

    static const std::unordered_set<std::string> CONSTANTS = {
        "true", "false", "null", "None", "Some", "Ok", "Err"
    };

    static const std::unordered_set<std::string> BUILTIN_FN = {
        "println", "print", "eprintln", "eprint",
        "panic", "assert", "assert_eq", "assert_ne",
        "format", "vec"
    };

    std::vector<HighlightSegment>
        Highlighter::highlightLine(const std::string& line, bool& inBlockComment)
    {
        std::vector<HighlightSegment> segments;
        size_t i = 0;
        size_t n = line.size();

        auto addSegment = [&](HighlightType type, size_t start, size_t len) {
            if (len > 0)
                segments.push_back({ type, start, len });
            };

        bool expectFunctionName = false;

        while (i < n) {
            // Внутри блочного комментария
            if (inBlockComment) {
                size_t start = i;
                while (i < n) {
                    if (i + 1 < n && line[i] == '*' && line[i + 1] == '/') {
                        i += 2;
                        inBlockComment = false;
                        break;
                    }
                    ++i;
                }
                addSegment(HighlightType::Comment, start, i - start);
                continue;
            }

            char c = line[i];

            if (c == ' ' || c == '\t') { ++i; continue; }

            // // comment
            if (c == '/' && i + 1 < n && line[i + 1] == '/') {
                bool isDoc = (i + 2 < n && line[i + 2] == '/');
                addSegment(isDoc ? HighlightType::DocComment : HighlightType::Comment,
                    i, n - i);
                i = n;
                continue;
            }

            // /* block */
            if (c == '/' && i + 1 < n && line[i + 1] == '*') {
                size_t start = i;
                i += 2;
                bool closed = false;
                while (i < n) {
                    if (i + 1 < n && line[i] == '*' && line[i + 1] == '/') {
                        i += 2;
                        closed = true;
                        break;
                    }
                    ++i;
                }
                if (!closed) inBlockComment = true;
                addSegment(HighlightType::Comment, start, i - start);
                continue;
            }

            // # preprocessor
            if (c == '#' && i == 0) {
                addSegment(HighlightType::Preprocessor, i, n - i);
                i = n;
                continue;
            }

            // "string"
            if (c == '"') {
                size_t start = i;
                ++i;
                while (i < n) {
                    if (line[i] == '\\' && i + 1 < n) { i += 2; continue; }
                    if (line[i] == '"') { ++i; break; }
                    ++i;
                }
                addSegment(HighlightType::String, start, i - start);
                continue;
            }

            // 'char'
            if (c == '\'') {
                size_t start = i;
                ++i;
                while (i < n) {
                    if (line[i] == '\\' && i + 1 < n) { i += 2; continue; }
                    if (line[i] == '\'') { ++i; break; }
                    ++i;
                }
                addSegment(HighlightType::Char, start, i - start);
                continue;
            }

            // numbers
            if (std::isdigit((unsigned char)c)) {
                size_t start = i;
                while (i < n && (std::isalnum((unsigned char)line[i]) ||
                    line[i] == '_' || line[i] == '.')) {
                    ++i;
                }
                addSegment(HighlightType::Number, start, i - start);
                continue;
            }

            // identifiers / keywords
            if (std::isalpha((unsigned char)c) || c == '_') {
                size_t start = i;
                while (i < n && (std::isalnum((unsigned char)line[i]) || line[i] == '_'))
                    ++i;
                std::string word = line.substr(start, i - start);

                HighlightType type = HighlightType::Identifier;

                if (KEYWORDS.count(word)) {
                    type = HighlightType::Keyword;
                    if (word == "fn") expectFunctionName = true;
                }
                else if (TYPES.count(word)) {
                    type = HighlightType::Type;
                }
                else if (CONSTANTS.count(word)) {
                    type = HighlightType::Constant;
                }
                else if (BUILTIN_FN.count(word)) {
                    type = HighlightType::Function;
                }
                else if (expectFunctionName) {
                    type = HighlightType::Function;
                    expectFunctionName = false;
                }

                addSegment(type, start, i - start);
                continue;
            }

            // operators / punctuation
            {
                size_t start = i;
                auto match = [&](const char* op) -> bool {
                    size_t len = std::strlen(op);
                    if (i + len <= n && line.compare(i, len, op) == 0) {
                        i += len;
                        return true;
                    }
                    return false;
                    };

                if (match("<<=") || match(">>=") || match("..=") ||
                    match("==") || match("!=") || match("<=") ||
                    match(">=") || match("&&") || match("||") ||
                    match("<<") || match(">>") || match("+=") ||
                    match("-=") || match("*=") || match("/=") ||
                    match("%=") || match("&=") || match("|=") ||
                    match("^=") || match("->") || match("=>") ||
                    match("::") || match("..")) {
                    addSegment(HighlightType::Operator, start, i - start);
                    continue;
                }

                const char* opChars = "+-*/%=<>!&|^~?:";
                const char* punctChars = "(){}[];,.:";
                if (std::strchr(opChars, c)) {
                    ++i;
                    addSegment(HighlightType::Operator, start, i - start);
                }
                else if (std::strchr(punctChars, c)) {
                    ++i;
                    addSegment(HighlightType::Punctuation, start, i - start);
                }
                else {
                    ++i;
                    addSegment(HighlightType::Default, start, i - start);
                }
                continue;
            }
        }

        return segments;
    }

} // namespace mayami