/**
 * NeoEnv.h — Hierarchical symbol table for NeoLanguage lexical scoping.
 *
 * Each NeoEnv represents a single lexical scope (global, function body,
 * or block). Scopes chain via parent pointers forming a singly-linked
 * list that is traversed during symbol lookup.
 *
 * Lookup strategy (standard lexical scoping):
 *   1. Search _table in this scope.
 *   2. If not found, recurse to parent.
 *   3. If no parent, the symbol is undefined.
 *
 * Assignment strategy:
 *   assign() walks the chain to find the nearest existing binding
 *   and updates it in-place.  If no binding exists anywhere,
 *   it creates a new entry in the global (root) scope.
 *
 * define() always creates a fresh binding in THIS scope, even if a
 * binding with the same name exists in an outer scope (shadowing).
 *
 * Memory: NeoEnv objects are typically stack-allocated (interpreter
 *         creates them on the C++ stack for each scope).  The _table
 *         uses PSRAMAllocator for its internal storage.
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage Phase 2 (Interpreter)
 */
#pragma once

#include <string>
#include <unordered_map>
#include "../math/cas/PSRAMAllocator.h"
#include "NeoValue.h"

// ════════════════════════════════════════════════════════════════════
// NeoEnv — single lexical scope node
// ════════════════════════════════════════════════════════════════════

class NeoEnv {
public:
    // ── Construction ─────────────────────────────────────────────
    explicit NeoEnv(NeoEnv* parent = nullptr) : _parent(parent) {}

    // Non-copyable (environments should not be accidentally copied)
    NeoEnv(const NeoEnv&)            = delete;
    NeoEnv& operator=(const NeoEnv&) = delete;

    // ── Parent access ─────────────────────────────────────────────
    NeoEnv* parent() const { return _parent; }

    // ── lookup — search this scope and all parents ─────────────────
    /**
     * Find a symbol, searching from this scope up through parents.
     * Returns nullptr if the symbol is not defined in any accessible scope.
     */
    const NeoValue* lookup(const std::string& name) const;
          NeoValue* lookup(const std::string& name);

    // ── define — create a new binding in THIS scope ────────────────
    /**
     * Define `name` in this scope with value `val`.
     * Shadows any binding with the same name in an outer scope.
     */
    void define(const std::string& name, NeoValue val);

    // ── assign — update nearest existing binding ────────────────────
    /**
     * Walk the scope chain and update the first binding for `name`.
     * If no binding is found anywhere, creates one in this scope.
     */
    void assign(const std::string& name, NeoValue val);

    // ── contains (this scope only) ────────────────────────────────
    bool contains(const std::string& name) const {
        return _table.find(name) != _table.end();
    }

private:
    // Unordered map backed by PSRAMAllocator
    using Table = std::unordered_map<
        std::string,
        NeoValue,
        std::hash<std::string>,
        std::equal_to<std::string>,
        cas::PSRAMAllocator<std::pair<const std::string, NeoValue>>
    >;

    NeoEnv* _parent;
    Table   _table;
};
