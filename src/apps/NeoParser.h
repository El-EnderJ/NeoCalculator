/**
 * NeoParser.h — Recursive-descent + Pratt expression parser for NeoLanguage.
 *
 * Parses a flat token stream (produced by NeoLexer) into an AST whose
 * nodes are allocated in a caller-supplied NeoArena.
 *
 * Features:
 *   · Pratt (top-down operator precedence) for expressions
 *   · Handles if/elif/else, while, for-in, def, return
 *   · Both `x = expr` and `x := expr` (delayed) assignment
 *   · Both `def f(x): ...` and `f(x) := expr` function definitions
 *   · Panic-mode error recovery: syncToNextStatement()
 *   · Iterative program-body loop (no recursion over statement list)
 *   · All nodes allocated via NeoArena — zero heap_new/delete
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage frontend
 */
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "NeoAST.h"
#include "NeoLexer.h"
#include "../math/cas/PSRAMAllocator.h"

// ════════════════════════════════════════════════════════════════════
// NeoParser
// ════════════════════════════════════════════════════════════════════

class NeoParser {
public:
    using TokenVec = std::vector<NeoToken, cas::PSRAMAllocator<NeoToken>>;

    /**
     * Construct with an arena for AST allocation.
     * The arena must outlive the parser and all produced AST nodes.
     */
    explicit NeoParser(NeoArena& arena);

    /**
     * Parse the token stream and return the root ProgramNode.
     * Returns nullptr only on catastrophic allocation failure.
     * Parse errors are recorded internally; use hasError()/lastError().
     */
    NeoNode* parse(const TokenVec& tokens);

    /** True if any parse error occurred. */
    bool hasError() const { return _hasError; }

    /** Description of the most-recent parse error. */
    const std::string& lastError() const { return _lastError; }

    /** Total number of errors recorded during the last parse. */
    int errorCount() const { return _errorCount; }

private:
    NeoArena&        _arena;
    const TokenVec*  _tokens;   ///< Pointer to token stream (valid during parse())
    size_t           _pos;      ///< Current read position in token stream
    bool             _hasError;
    std::string      _lastError;
    int              _errorCount;

    // ── Token access ─────────────────────────────────────────────
    const NeoToken& current() const;
    const NeoToken& peek(int offset = 0) const;
    bool  check(NeoTokType t) const;
    bool  checkNext(NeoTokType t) const;
    bool  atEnd() const;
    const NeoToken& advance();
    bool  match(NeoTokType t);          ///< Consume token if type matches; return true
    const NeoToken& expect(NeoTokType t, const char* context); ///< Advance or record error

    // ── Error handling ────────────────────────────────────────────
    void recordError(const std::string& msg, int line, int col);
    void syncToNextStatement(); ///< Panic-mode: skip to next statement boundary

    // ── Statement parsers ─────────────────────────────────────────
    NeoNode* parseStatement();
    NeoNode* parseFunctionDef();
    NeoNode* parseIf();
    NeoNode* parseWhile();
    NeoNode* parseFor();
    NeoNode* parseReturn();
    NeoNode* parseTryExcept();   ///< Phase 6: try/except
    NeoNode* parseAssignmentOrExpr();

    /**
     * parseBlock — parse an INDENT-indented block of statements.
     * Expects INDENT already consumed or about to be consumed.
     * Returns statements into `out`.
     */
    void parseBlock(NeoNodeVec& out);

    // ── Expression parsers (Pratt) ────────────────────────────────
    NeoNode* parseExpression(int minPrec = 0);
    NeoNode* parsePrimary();
    NeoNode* parseUnary();
    NeoNode* parseCall(NeoNode* callee, const NeoToken& nameTok);

    // ── Operator precedence helpers ───────────────────────────────

    /** Returns the infix precedence of the current token, or -1. */
    int  infixPrecedence(const NeoToken& t) const;

    /** Returns true if the operator is right-associative. */
    bool isRightAssoc(const NeoToken& t) const;

    /** Convert a token to a BinaryOpNode::OpKind (pre-checked). */
    BinaryOpNode::OpKind tokenToBinOp(const NeoToken& t) const;

    // ── Utility ───────────────────────────────────────────────────
    /** Skip over NEWLINE tokens without consuming other tokens. */
    void skipNewlines();

    /** True if identifier `id` looks like a function-definition header
     *  using the `f(x) := expr` one-liner syntax, i.e. IDENTIFIER LPAREN. */
    bool looksLikeFuncDef() const;

    /**
     * Parse a subscript operation: target[index] or target[row, col].
     * Called after the target node has already been parsed.
     * Consumes '[', the index expression(s), and ']'.
     */
    NeoNode* parseIndexOp(NeoNode* target, int line, int col);
};
