// src/lexer/Token.h
#pragma once

#include <string>

namespace mayami {

    enum class TokenType {
        EndOfFile,
        Unknown,

        // Литералы
        Integer, Float, String, Char, True, False,

        // Идентификаторы
        Identifier,

        // Ключевые слова
        KwFn, KwLet, KwMut, KwIf, KwElse, KwWhile, KwFor, KwLoop,
        KwBreak, KwContinue, KwReturn, KwStruct, KwEnum, KwTrait,
        KwImpl, KwMatch, KwAsync, KwAwait, KwPub, KwUse, KwMod,
        KwConst, KwStatic, KwIn, KwAs, KwSelf, KwSelfType, KwWhere,

        // Типы
        KwI8, KwI16, KwI32, KwI64,
        KwU8, KwU16, KwU32, KwU64,
        KwF32, KwF64,
        KwBool, KwString, KwChar, KwVoid,

        // Операторы
        Plus, Minus, Star, Slash, Percent,
        Assign, Eq, NotEq, Lt, Gt, LtEq, GtEq,
        And, Or, Not, Amp, Pipe, Caret, Tilde,
        Shl, Shr,
        PlusEq, MinusEq, StarEq, SlashEq, PercentEq,
        AmpEq, PipeEq, CaretEq, ShlEq, ShrEq,
        Arrow, FatArrow, ColonColon,
        Dot, DotDot, DotDotEq, Question,

        // Разделители
        LParen, RParen, LBrace, RBrace, LBracket, RBracket,
        Comma, Semi, Colon,
    };

    struct Token {
        TokenType type;
        std::string lexeme;
        int line;
        int column;

        Token() : type(TokenType::Unknown), line(0), column(0) {}
        Token(TokenType t, const std::string& lx, int ln, int col)
            : type(t), lexeme(lx), line(ln), column(col) {
        }
    };

    std::string tokenTypeName(TokenType type);

} // namespace mayami