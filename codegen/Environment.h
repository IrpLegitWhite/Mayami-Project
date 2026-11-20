// src/codegen/Environment.h
#pragma once

#include "Value.h"
#include <unordered_map>
#include <string>
#include <memory>

namespace mayami {

    class Environment : public std::enable_shared_from_this<Environment> {
    public:
        explicit Environment(std::shared_ptr<Environment> parent = nullptr)
            : m_parent(std::move(parent)) {
        }

        void define(const std::string& name, ValuePtr value) {
            m_vars[name] = std::move(value);
        }

        ValuePtr get(const std::string& name) {
            auto it = m_vars.find(name);
            if (it != m_vars.end()) return it->second;
            if (m_parent) return m_parent->get(name);
            return nullptr;
        }

        bool assign(const std::string& name, ValuePtr value) {
            auto it = m_vars.find(name);
            if (it != m_vars.end()) {
                it->second = std::move(value);
                return true;
            }
            if (m_parent) return m_parent->assign(name, value);
            return false;
        }

        bool has(const std::string& name) const {
            if (m_vars.count(name)) return true;
            if (m_parent) return m_parent->has(name);
            return false;
        }

    private:
        std::unordered_map<std::string, ValuePtr> m_vars;
        std::shared_ptr<Environment> m_parent;
    };

} // namespace mayami