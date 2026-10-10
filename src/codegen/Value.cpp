// src/codegen/Value.cpp
#include "Value.h"
#include <cstdio>

namespace mayami {

    std::string Value::toString() const {
        switch (type) {
        case ValueType::Null:   return "null";
        case ValueType::Int:    return std::to_string(i);
        case ValueType::Float: {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%g", f);
            return buf;
        }
        case ValueType::Bool:   return b ? "true" : "false";
        case ValueType::String: return s;
        case ValueType::Char:   return s;

        case ValueType::Struct: {
            std::string result = structName + " { ";
            if (fields) {
                bool first = true;
                for (const auto& kv : *fields) {
                    if (!first) result += ", ";
                    result += kv.first + ": ";
                    result += kv.second ? kv.second->toString() : "null";
                    first = false;
                }
            }
            result += " }";
            return result;
        }

        case ValueType::Array: {   // ★
            std::string r = "[";
            if (elements) {
                for (size_t i = 0; i < elements->size(); ++i) {
                    r += (*elements)[i] ? (*elements)[i]->toString() : "null";
                    if (i + 1 < elements->size()) r += ", ";
                }
            }
            r += "]";
            return r;
        }
        }
        return "?";
    }

    ValuePtr makeInt(long long v) { return std::make_shared<Value>(v); }
    ValuePtr makeFloat(double v) { return std::make_shared<Value>(v); }
    ValuePtr makeBool(bool v) { return std::make_shared<Value>(v); }
    ValuePtr makeString(const std::string& v) { return std::make_shared<Value>(v); }
    ValuePtr makeChar(char v) { return std::make_shared<Value>(v); }
    ValuePtr makeNull() { return std::make_shared<Value>(); }

    ValuePtr makeStruct(const std::string& structName) {
        auto v = std::make_shared<Value>();
        v->type = ValueType::Struct;
        v->structName = structName;
        v->fields = std::make_shared<std::unordered_map<std::string, ValuePtr>>();
        return v;
    }

    ValuePtr makeArray() {   // ★
        auto v = std::make_shared<Value>();
        v->type = ValueType::Array;
        v->elements = std::make_shared<std::vector<ValuePtr>>();
        return v;
    }

} // namespace mayami