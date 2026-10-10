// src/vm/VM.h
#pragma once

#include "Bytecode.h"
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include <cstdint>

namespace mayami {

    struct VmValue {
        VmType type = VmType::Null;
        int64_t     i = 0;
        double      f = 0.0;
        bool        b = false;
        std::string s;

        std::shared_ptr<std::vector<VmValue>> array;
        uint32_t structId = 0;
        std::shared_ptr<std::unordered_map<uint32_t, VmValue>> fields;

        VmValue() {}
        VmValue(int64_t v) : type(VmType::Int), i(v) {}
        VmValue(double v) : type(VmType::Float), f(v) {}
        VmValue(bool v) : type(VmType::Bool), b(v) {}

        std::string toString() const;
        double toNumber() const;
    };

    struct VmRuntimeError { std::string message; };
    struct VmReturnSignal { VmValue value; };

    using VmOutputCallback = std::function<void(const std::string&)>;

    struct Frame {
        uint32_t returnIP = 0;
        uint32_t varBase = 0;
        uint32_t funcIdx = 0;
    };

    class VM {
    public:
        VM();
        ~VM();

        void setOutputCallback(VmOutputCallback cb) { m_output = std::move(cb); }

        // ★ Отладка
        void setDebug(bool on) { m_debug = on; }
        void setDebugCallback(std::function<void(const std::string&)> cb) {
            m_debugOut = std::move(cb);
        }
        bool isDebug() const { return m_debug; }

        bool load(const std::vector<uint8_t>& data);
        bool run();

        const std::vector<std::string>& errors() const { return m_errors; }
        bool hasErrors() const { return !m_errors.empty(); }

    private:
        void push(const VmValue& v) { m_stack.push_back(v); }
        VmValue pop();
        VmValue& peek(int offset = 0);

        bool executeOne();

        uint8_t  readByte();
        uint16_t readU16();
        uint32_t readU32();
        int64_t  readI64();
        double   readF64();
        std::string readString();

        void error(const std::string& msg);

        VmValue callNative(uint32_t id, const std::vector<VmValue>& args);
        static std::string formatValue(const VmValue& v);

        std::string stringAt(uint32_t offset);

        std::vector<uint8_t> m_code;
        size_t               m_ip = 0;

        std::vector<uint8_t> m_constants;
        std::vector<VmValue> m_stack;
        std::vector<VmValue> m_vars;
        std::vector<Frame>   m_frames;

        std::vector<BytecodeFunction> m_functions;

        uint32_t m_entryPoint = 0;

        VmOutputCallback m_output;
        std::vector<std::string> m_errors;

        // ★ Отладка
        bool m_debug = false;
        std::function<void(const std::string&)> m_debugOut;
        size_t m_stepCount = 0;
    };

} // namespace mayami