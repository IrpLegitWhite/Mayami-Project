// src/vm/Bytecode.h
#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace mayami {

    constexpr uint32_t BYTECODE_MAGIC = 0x4D594D49;
    constexpr uint16_t BYTECODE_VERSION = 1;

    enum class OpCode : uint8_t {
        NOP = 0x00,
        PUSH_NULL = 0x01,
        PUSH_TRUE = 0x02,
        PUSH_FALSE = 0x03,
        PUSH_INT = 0x04,
        PUSH_FLOAT = 0x05,
        PUSH_STR = 0x06,
        PUSH_CHAR = 0x07,

        POP = 0x08,
        DUP = 0x09,
        SWAP = 0x0A,

        LOAD_LOCAL = 0x10,
        STORE_LOCAL = 0x11,
        LOAD_GLOBAL = 0x12,
        STORE_GLOBAL = 0x13,
        LOAD_VAR = 0x14,
        STORE_VAR = 0x15,

        ADD = 0x20,
        SUB = 0x21,
        MUL = 0x22,
        DIV = 0x23,
        MOD = 0x24,
        NEG = 0x25,

        EQ = 0x30,
        NEQ = 0x31,
        LT = 0x32,
        GT = 0x33,
        LE = 0x34,
        GE = 0x35,

        AND = 0x40,
        OR = 0x41,
        NOT = 0x42,
        BIT_AND = 0x43,
        BIT_OR = 0x44,
        BIT_XOR = 0x45,
        BIT_NOT = 0x46,
        SHL = 0x47,
        SHR = 0x48,

        JUMP = 0x50,
        JUMP_IF_FALSE = 0x51,
        JUMP_IF_TRUE = 0x52,

        CALL = 0x60,
        RET = 0x61,
        CALL_NATIVE = 0x62,

        NEW_ARRAY = 0x70,
        ARRAY_GET = 0x71,
        ARRAY_SET = 0x72,
        ARRAY_LEN = 0x73,

        NEW_STRUCT = 0x80,
        FIELD_GET = 0x81,
        FIELD_SET = 0x82,

        ITER_NEW = 0x90,
        ITER_NEXT = 0x91,
        ITER_GET = 0x92,

        HALT = 0xFF,
    };

    enum NativeId : uint32_t {
        NATIVE_PRINTLN = 1,
        NATIVE_PRINT = 2,
        NATIVE_EPRINT = 3,

        NATIVE_SQRT = 10,
        NATIVE_ABS = 11,
        NATIVE_SIN = 12,
        NATIVE_COS = 13,
        NATIVE_TAN = 14,
        NATIVE_POW = 15,
        NATIVE_FLOOR = 16,
        NATIVE_CEIL = 17,
        NATIVE_ROUND = 18,
        NATIVE_MIN = 19,
        NATIVE_MAX = 20,

        NATIVE_STR_LEN = 30,
        NATIVE_STR_UPPER = 31,
        NATIVE_STR_LOWER = 32,
        NATIVE_STR_SUBSTR = 33,
        NATIVE_STR_FIND = 34,
        NATIVE_STR_CONTAINS = 35,
        NATIVE_STR_REPLACE = 36,

        NATIVE_UI_BEGIN = 100,
        NATIVE_UI_LABEL = 101,
        NATIVE_UI_BUTTON = 102,
        NATIVE_UI_INPUT = 103,
        NATIVE_UI_CHECKBOX = 104,
        NATIVE_UI_SHOW = 105,
        NATIVE_UI_CLICKED = 106,
        NATIVE_UI_GETVAL = 107,
        NATIVE_UI_TEXTBOX = 108,

        NATIVE_UI_RUNNING = 109,
        NATIVE_UI_POLL = 110,
        NATIVE_UI_SLEEP = 111,
        NATIVE_UI_SET_LABEL = 112,
        NATIVE_UI_SET_TEXT = 113,
        NATIVE_UI_REMOVE = 114,
        NATIVE_UI_REFRESH = 115,

        NATIVE_UI_THEME = 120,
        NATIVE_UI_RECT = 121,
        NATIVE_UI_RECT_ROUND = 122,
        NATIVE_UI_TEXT = 123,
        NATIVE_UI_TEXT_SIZE = 124,
        NATIVE_UI_BUTTON_C = 125,
        NATIVE_UI_CLEAR = 126,
        NATIVE_UI_WIDTH = 127,
        NATIVE_UI_HEIGHT = 128,
        NATIVE_UI_MOUSE_X = 129,
        NATIVE_UI_MOUSE_Y = 130,
    };

    enum class VmType : uint8_t {
        Null = 0,
        Int = 1,
        Float = 2,
        Bool = 3,
        String = 4,
        Char = 5,
        Array = 6,
        Struct = 7,
    };

#pragma pack(push, 1)
    struct BytecodeHeader {
        uint32_t magic;
        uint16_t version;
        uint16_t reserved;
        uint32_t codeSize;
        uint32_t constSize;
        uint32_t funcCount;
        uint32_t structCount;
        uint32_t entryPoint;
    };
#pragma pack(pop)

#pragma pack(push, 1)
    struct BytecodeFunction {
        uint32_t nameOffset;
        uint32_t codeOffset;
        uint32_t localCount;
        uint8_t  paramCount;
        uint8_t  reserved[3];
    };
#pragma pack(pop)

#pragma pack(push, 1)
    struct BytecodeStruct {
        uint32_t nameOffset;
        uint32_t fieldCount;
    };
#pragma pack(pop)

    inline void writeU16(std::vector<uint8_t>& v, uint16_t x) {
        v.push_back((uint8_t)(x & 0xFF));
        v.push_back((uint8_t)((x >> 8) & 0xFF));
    }
    inline void writeU32(std::vector<uint8_t>& v, uint32_t x) {
        v.push_back((uint8_t)(x & 0xFF));
        v.push_back((uint8_t)((x >> 8) & 0xFF));
        v.push_back((uint8_t)((x >> 16) & 0xFF));
        v.push_back((uint8_t)((x >> 24) & 0xFF));
    }
    inline void writeU64(std::vector<uint8_t>& v, uint64_t x) {
        for (int i = 0; i < 8; ++i)
            v.push_back((uint8_t)((x >> (i * 8)) & 0xFF));
    }
    inline void writeI64(std::vector<uint8_t>& v, int64_t x) {
        writeU64(v, (uint64_t)x);
    }
    inline void writeF64(std::vector<uint8_t>& v, double x) {
        uint64_t u;
        std::memcpy(&u, &x, 8);
        writeU64(v, u);
    }
    inline void writeBytes(std::vector<uint8_t>& v, const std::string& s) {
        writeU32(v, (uint32_t)s.size());
        v.insert(v.end(), s.begin(), s.end());
    }

    inline uint16_t readU16(const uint8_t* p) {
        return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
    }
    inline uint32_t readU32(const uint8_t* p) {
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
            ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }
    inline uint64_t readU64(const uint8_t* p) {
        uint64_t r = 0;
        for (int i = 0; i < 8; ++i)
            r |= ((uint64_t)p[i]) << (i * 8);
        return r;
    }
    inline int64_t readI64(const uint8_t* p) {
        return (int64_t)readU64(p);
    }
    inline double readF64(const uint8_t* p) {
        uint64_t u = readU64(p);
        double d;
        std::memcpy(&d, &u, 8);
        return d;
    }

    // ============================================================
    // Имена опкодов (для отладки)
    // ============================================================
    inline const char* OpCodeName(OpCode op) {
        switch (op) {
        case OpCode::NOP:            return "NOP";
        case OpCode::PUSH_NULL:      return "PUSH_NULL";
        case OpCode::PUSH_TRUE:      return "PUSH_TRUE";
        case OpCode::PUSH_FALSE:     return "PUSH_FALSE";
        case OpCode::PUSH_INT:       return "PUSH_INT";
        case OpCode::PUSH_FLOAT:     return "PUSH_FLOAT";
        case OpCode::PUSH_STR:       return "PUSH_STR";
        case OpCode::PUSH_CHAR:      return "PUSH_CHAR";
        case OpCode::POP:            return "POP";
        case OpCode::DUP:            return "DUP";
        case OpCode::SWAP:           return "SWAP";
        case OpCode::LOAD_LOCAL:     return "LOAD_LOCAL";
        case OpCode::STORE_LOCAL:    return "STORE_LOCAL";
        case OpCode::LOAD_GLOBAL:    return "LOAD_GLOBAL";
        case OpCode::STORE_GLOBAL:   return "STORE_GLOBAL";
        case OpCode::LOAD_VAR:       return "LOAD_VAR";
        case OpCode::STORE_VAR:      return "STORE_VAR";
        case OpCode::ADD:            return "ADD";
        case OpCode::SUB:            return "SUB";
        case OpCode::MUL:            return "MUL";
        case OpCode::DIV:            return "DIV";
        case OpCode::MOD:            return "MOD";
        case OpCode::NEG:            return "NEG";
        case OpCode::EQ:             return "EQ";
        case OpCode::NEQ:            return "NEQ";
        case OpCode::LT:             return "LT";
        case OpCode::GT:             return "GT";
        case OpCode::LE:             return "LE";
        case OpCode::GE:             return "GE";
        case OpCode::AND:            return "AND";
        case OpCode::OR:             return "OR";
        case OpCode::NOT:            return "NOT";
        case OpCode::BIT_AND:        return "BIT_AND";
        case OpCode::BIT_OR:         return "BIT_OR";
        case OpCode::BIT_XOR:        return "BIT_XOR";
        case OpCode::BIT_NOT:        return "BIT_NOT";
        case OpCode::SHL:            return "SHL";
        case OpCode::SHR:            return "SHR";
        case OpCode::JUMP:           return "JUMP";
        case OpCode::JUMP_IF_FALSE:  return "JUMP_IF_FALSE";
        case OpCode::JUMP_IF_TRUE:   return "JUMP_IF_TRUE";
        case OpCode::CALL:           return "CALL";
        case OpCode::RET:            return "RET";
        case OpCode::CALL_NATIVE:    return "CALL_NATIVE";
        case OpCode::NEW_ARRAY:      return "NEW_ARRAY";
        case OpCode::ARRAY_GET:      return "ARRAY_GET";
        case OpCode::ARRAY_SET:      return "ARRAY_SET";
        case OpCode::ARRAY_LEN:      return "ARRAY_LEN";
        case OpCode::NEW_STRUCT:     return "NEW_STRUCT";
        case OpCode::FIELD_GET:      return "FIELD_GET";
        case OpCode::FIELD_SET:      return "FIELD_SET";
        case OpCode::ITER_NEW:       return "ITER_NEW";
        case OpCode::ITER_NEXT:      return "ITER_NEXT";
        case OpCode::ITER_GET:       return "ITER_GET";
        case OpCode::HALT:           return "HALT";
        }
        return "UNKNOWN";
    }

} // namespace mayami