// src/codegen/Value.h
#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

namespace mayami {

    enum class ValueType {
        Null,
        Int,
        Float,
        Bool,
        String,
        Char,
        Struct,
        Array,      // ★
    };

    struct Value {
        ValueType type = ValueType::Null;

        long long   i = 0;
        double      f = 0.0;
        bool        b = false;
        std::string s;

        // Struct
        std::string structName;
        std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<Value>>> fields;

        // Array
        std::shared_ptr<std::vector<std::shared_ptr<Value>>> elements;   // ★

        Value() : type(ValueType::Null) {}
        explicit Value(long long v) : type(ValueType::Int), i(v) {}
        explicit Value(double v) : type(ValueType::Float), f(v) {}
        explicit Value(bool v) : type(ValueType::Bool), b(v) {}
        explicit Value(const std::string& v) : type(ValueType::String), s(v) {}
        explicit Value(char v) : type(ValueType::Char), s(1, v) {}

        bool isNull()   const { return type == ValueType::Null; }
        bool isInt()    const { return type == ValueType::Int; }
        bool isFloat()  const { return type == ValueType::Float; }
        bool isBool()   const { return type == ValueType::Bool; }
        bool isString() const { return type == ValueType::String; }
        bool isChar()   const { return type == ValueType::Char; }
        bool isStruct() const { return type == ValueType::Struct; }
        bool isArray()  const { return type == ValueType::Array; }   // ★
        bool isNumber() const { return isInt() || isFloat(); }

        double toNumber() const {
            switch (type) {
            case ValueType::Int:    return (double)i;
            case ValueType::Float:  return f;
            case ValueType::Bool:   return b ? 1.0 : 0.0;
            default:                return 0.0;
            }
        }

        std::string toString() const;
    };

    using ValuePtr = std::shared_ptr<Value>;

    ValuePtr makeInt(long long v);
    ValuePtr makeFloat(double v);
    ValuePtr makeBool(bool v);
    ValuePtr makeString(const std::string& v);
    ValuePtr makeChar(char v);
    ValuePtr makeNull();
    ValuePtr makeStruct(const std::string& structName);
    ValuePtr makeArray();   // ★

} // namespace mayami