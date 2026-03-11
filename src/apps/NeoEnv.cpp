/**
 * NeoEnv.cpp — Hierarchical symbol table implementation.
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage Phase 2 (Interpreter)
 */

#include "NeoEnv.h"

// ════════════════════════════════════════════════════════════════════
// lookup
// ════════════════════════════════════════════════════════════════════

const NeoValue* NeoEnv::lookup(const std::string& name) const {
    auto it = _table.find(name);
    if (it != _table.end()) return &it->second;
    if (_parent) return _parent->lookup(name);
    return nullptr;
}

NeoValue* NeoEnv::lookup(const std::string& name) {
    auto it = _table.find(name);
    if (it != _table.end()) return &it->second;
    if (_parent) return _parent->lookup(name);
    return nullptr;
}

// ════════════════════════════════════════════════════════════════════
// define — create binding in THIS scope
// ════════════════════════════════════════════════════════════════════

void NeoEnv::define(const std::string& name, NeoValue val) {
    _table[name] = std::move(val);
}

// ════════════════════════════════════════════════════════════════════
// assign — update nearest existing binding, or create here
// ════════════════════════════════════════════════════════════════════

void NeoEnv::assign(const std::string& name, NeoValue val) {
    // Walk up the chain looking for an existing binding
    NeoEnv* env = this;
    while (env) {
        auto it = env->_table.find(name);
        if (it != env->_table.end()) {
            it->second = std::move(val);
            return;
        }
        env = env->_parent;
    }
    // Not found anywhere → create in this scope
    _table[name] = std::move(val);
}
