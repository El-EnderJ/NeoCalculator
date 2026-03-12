/**
 * NeoParser.cpp — Recursive-descent + Pratt expression parser.
 *
 * All AST nodes are allocated from the NeoArena passed to the constructor.
 * The parser is iterative at the program-body level to avoid stack overflow
 * on long scripts (ESP32-S3 typical task stack is 8-16 KB).
 *
 * Operator precedence (low to high):
 *   1  or
 *   2  and
 *   3  == != < <= > >=      (comparison, infix)
 *   4  + -
 *   5  * /
 *   6  ^  **   (right-associative)
 *   Unary - / not: handled in parseUnary(), not part of the infix table
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage frontend
 */

#include "NeoParser.h"
#include <cstdio>
#include <cstdlib>   // strtod

// ═════════════════════════════════════════════════════════════════
// Constructor
// ═════════════════════════════════════════════════════════════════

NeoParser::NeoParser(NeoArena& arena)
    : _arena(arena)
    , _tokens(nullptr)
    , _pos(0)
    , _hasError(false)
    , _lastError("")
    , _errorCount(0)
{}

// ═════════════════════════════════════════════════════════════════
// parse() — entry point
// ═════════════════════════════════════════════════════════════════

NeoNode* NeoParser::parse(const TokenVec& tokens) {
    _tokens     = &tokens;
    _pos        = 0;
    _hasError   = false;
    _lastError  = "";
    _errorCount = 0;

    ProgramNode* prog = _arena.make<ProgramNode>(1, 1);
    if (!prog) return nullptr;

    // Iterative loop over top-level statements — no recursion depth issue
    skipNewlines();
    while (!atEnd()) {
        NeoNode* stmt = parseStatement();
        if (stmt) prog->statements.push_back(stmt);
        skipNewlines();
    }

    return prog;
}

// ═════════════════════════════════════════════════════════════════
// Token access helpers
// ═════════════════════════════════════════════════════════════════

const NeoToken& NeoParser::current() const {
    static const NeoToken eof(NeoTokType::END_OF_FILE, "", 0, 0);
    if (_pos < _tokens->size()) return (*_tokens)[_pos];
    return eof;
}

const NeoToken& NeoParser::peek(int offset) const {
    static const NeoToken eof(NeoTokType::END_OF_FILE, "", 0, 0);
    size_t p = _pos + static_cast<size_t>(offset);
    if (p < _tokens->size()) return (*_tokens)[p];
    return eof;
}

bool NeoParser::check(NeoTokType t) const { return current().type == t; }
bool NeoParser::checkNext(NeoTokType t) const { return peek(1).type == t; }
bool NeoParser::atEnd() const { return check(NeoTokType::END_OF_FILE); }

const NeoToken& NeoParser::advance() {
    const NeoToken& t = current();
    if (!atEnd()) _pos++;
    return t;
}

bool NeoParser::match(NeoTokType t) {
    if (check(t)) { advance(); return true; }
    return false;
}

const NeoToken& NeoParser::expect(NeoTokType t, const char* context) {
    if (check(t)) return advance();
    const NeoToken& cur = current();
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "expected token type %d (in %s) but got '%s' at line %d col %d",
        static_cast<int>(t), context,
        cur.value.c_str(), cur.line, cur.col);
    recordError(buf, cur.line, cur.col);
    // Return current token without advancing (so the caller can resync)
    return current();
}

// ═════════════════════════════════════════════════════════════════
// Error handling
// ═════════════════════════════════════════════════════════════════

void NeoParser::recordError(const std::string& msg, int line, int col) {
    _hasError = true;
    _errorCount++;
    // Keep only the first error message for simple display
    if (_lastError.empty()) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), " (line %d, col %d)", line, col);
        _lastError = msg + buf;
    }
}

/**
 * Panic-mode recovery: advance until we see a NEWLINE, DEDENT, or EOF.
 * This synchronises the parser to the next statement boundary.
 */
void NeoParser::syncToNextStatement() {
    while (!atEnd() &&
           !check(NeoTokType::NEWLINE) &&
           !check(NeoTokType::DEDENT)) {
        advance();
    }
    match(NeoTokType::NEWLINE);
}

// ═════════════════════════════════════════════════════════════════
// Utility
// ═════════════════════════════════════════════════════════════════

void NeoParser::skipNewlines() {
    while (check(NeoTokType::NEWLINE)) advance();
}

bool NeoParser::looksLikeFuncDef() const {
    // Pattern: IDENTIFIER LPAREN ... RPAREN DELAYED_ASSIGN
    // We do a simple lookahead scan.
    if (!check(NeoTokType::IDENTIFIER)) return false;
    // Walk forward to find matching ) followed by :=
    size_t i = _pos + 1;
    if (i >= _tokens->size() || (*_tokens)[i].type != NeoTokType::LPAREN) return false;
    int depth = 0;
    for (; i < _tokens->size(); ++i) {
        NeoTokType tt = (*_tokens)[i].type;
        if (tt == NeoTokType::LPAREN)  { depth++; continue; }
        if (tt == NeoTokType::RPAREN)  {
            depth--;
            if (depth == 0) {
                // Next token should be :=
                size_t j = i + 1;
                if (j < _tokens->size() && (*_tokens)[j].type == NeoTokType::DELAYED_ASSIGN)
                    return true;
                return false;
            }
        }
        if (tt == NeoTokType::NEWLINE || tt == NeoTokType::END_OF_FILE) return false;
    }
    return false;
}

// ═════════════════════════════════════════════════════════════════
// parseBlock — INDENT block...DEDENT
// ═════════════════════════════════════════════════════════════════

void NeoParser::parseBlock(NeoNodeVec& out) {
    if (!match(NeoTokType::INDENT)) {
        // Tolerate missing INDENT for one-liner after colon: `if x: stmt`
        skipNewlines();
        NeoNode* s = parseStatement();
        if (s) out.push_back(s);
        return;
    }
    skipNewlines();
    while (!atEnd() && !check(NeoTokType::DEDENT)) {
        NeoNode* s = parseStatement();
        if (s) out.push_back(s);
        skipNewlines();
    }
    match(NeoTokType::DEDENT);
}

// ═════════════════════════════════════════════════════════════════
// parseStatement — dispatch to specialised statement parsers
// ═════════════════════════════════════════════════════════════════

NeoNode* NeoParser::parseStatement() {
    skipNewlines();

    // ── def keyword ───────────────────────────────────────────────
    if (check(NeoTokType::DEF)) return parseFunctionDef();

    // ── One-liner function definition: f(x) := expr ──────────────
    if (looksLikeFuncDef()) return parseFunctionDef();

    // ── if ────────────────────────────────────────────────────────
    if (check(NeoTokType::IF)) return parseIf();

    // ── while ─────────────────────────────────────────────────────
    if (check(NeoTokType::WHILE)) return parseWhile();

    // ── for ───────────────────────────────────────────────────────
    if (check(NeoTokType::FOR)) return parseFor();

    // ── return ────────────────────────────────────────────────────
    if (check(NeoTokType::RETURN)) return parseReturn();

    // ── try/except ────────────────────────────────────────────────
    if (check(NeoTokType::TRY)) return parseTryExcept();

    // ── import / from...import ─────────────────────────────────────
    if (check(NeoTokType::IMPORT) || check(NeoTokType::FROM)) return parseImport();

    // ── assignment or expression statement ───────────────────────
    return parseAssignmentOrExpr();
}

// ═════════════════════════════════════════════════════════════════
// parseFunctionDef
// ═════════════════════════════════════════════════════════════════

NeoNode* NeoParser::parseFunctionDef() {
    // Two syntaxes:
    //   A. def f(params):  NEWLINE INDENT body DEDENT
    //   B. f(params) := expr   (one-liner)

    bool isDefKeyword = check(NeoTokType::DEF);
    int tl = current().line, tc = current().col;

    if (isDefKeyword) advance(); // consume 'def'

    const NeoToken& nameTok = expect(NeoTokType::IDENTIFIER, "function definition");
    std::string name = nameTok.value;

    FunctionDefNode* node = _arena.make<FunctionDefNode>(name, !isDefKeyword, tl, tc);
    if (!node) return nullptr;

    // Parameter list
    expect(NeoTokType::LPAREN, "function parameter list");
    while (!check(NeoTokType::RPAREN) && !atEnd()) {
        const NeoToken& param = expect(NeoTokType::IDENTIFIER, "parameter name");
        node->params.push_back(param.value);
        if (!match(NeoTokType::COMMA)) break;
    }
    expect(NeoTokType::RPAREN, "function parameter list end");

    if (isDefKeyword) {
        // Traditional def: colon + indented body
        expect(NeoTokType::COLON, "function definition");
        match(NeoTokType::NEWLINE);
        parseBlock(node->body);
    } else {
        // One-liner: f(x) := expr
        expect(NeoTokType::DELAYED_ASSIGN, "one-liner function definition");
        NeoNode* expr = parseExpression();
        if (expr) node->body.push_back(expr);
        match(NeoTokType::NEWLINE);
    }

    return node;
}

// ═════════════════════════════════════════════════════════════════
// parseIf
// ═════════════════════════════════════════════════════════════════

NeoNode* NeoParser::parseIf() {
    int tl = current().line, tc = current().col;
    advance(); // consume 'if'

    NeoNode* cond = parseExpression();
    if (!cond) { syncToNextStatement(); return nullptr; }

    expect(NeoTokType::COLON, "if statement");
    match(NeoTokType::NEWLINE);

    IfNode* node = _arena.make<IfNode>(cond, tl, tc);
    if (!node) return nullptr;
    parseBlock(node->then_body);

    // elif / else chain — iterative to avoid deep recursion
    IfNode* current_if = node;
    while (true) {
        skipNewlines();
        if (check(NeoTokType::ELIF)) {
            int el = current().line, ec = current().col;
            advance(); // consume 'elif'
            NeoNode* elif_cond = parseExpression();
            if (!elif_cond) { syncToNextStatement(); break; }
            expect(NeoTokType::COLON, "elif statement");
            match(NeoTokType::NEWLINE);
            IfNode* elif_node = _arena.make<IfNode>(elif_cond, el, ec);
            if (!elif_node) break;
            parseBlock(elif_node->then_body);
            current_if->else_body.push_back(elif_node);
            current_if = elif_node;
        } else if (check(NeoTokType::ELSE)) {
            advance(); // consume 'else'
            expect(NeoTokType::COLON, "else clause");
            match(NeoTokType::NEWLINE);
            parseBlock(current_if->else_body);
            break;
        } else {
            break;
        }
    }

    return node;
}

// ═════════════════════════════════════════════════════════════════
// parseWhile
// ═════════════════════════════════════════════════════════════════

NeoNode* NeoParser::parseWhile() {
    int tl = current().line, tc = current().col;
    advance(); // consume 'while'

    NeoNode* cond = parseExpression();
    if (!cond) { syncToNextStatement(); return nullptr; }

    expect(NeoTokType::COLON, "while statement");
    match(NeoTokType::NEWLINE);

    WhileNode* node = _arena.make<WhileNode>(cond, tl, tc);
    if (!node) return nullptr;
    parseBlock(node->body);
    return node;
}

// ═════════════════════════════════════════════════════════════════
// parseFor
// ═════════════════════════════════════════════════════════════════

NeoNode* NeoParser::parseFor() {
    int tl = current().line, tc = current().col;
    advance(); // consume 'for'

    const NeoToken& varTok = expect(NeoTokType::IDENTIFIER, "for-in variable");
    std::string var = varTok.value;

    expect(NeoTokType::IN, "for-in statement");

    NeoNode* iter = parseExpression();
    if (!iter) { syncToNextStatement(); return nullptr; }

    expect(NeoTokType::COLON, "for-in statement");
    match(NeoTokType::NEWLINE);

    ForInNode* node = _arena.make<ForInNode>(var, iter, tl, tc);
    if (!node) return nullptr;
    parseBlock(node->body);
    return node;
}

// ═════════════════════════════════════════════════════════════════
// parseReturn
// ═════════════════════════════════════════════════════════════════

NeoNode* NeoParser::parseReturn() {
    int tl = current().line, tc = current().col;
    advance(); // consume 'return'

    NeoNode* val = nullptr;
    if (!check(NeoTokType::NEWLINE) && !check(NeoTokType::END_OF_FILE) && !check(NeoTokType::SEMICOLON)) {
        val = parseExpression();
    }
    match(NeoTokType::NEWLINE);

    return _arena.make<ReturnNode>(val, tl, tc);
}

// ═════════════════════════════════════════════════════════════════
// parseTryExcept — try: ... except [Type] as var: ...
// ═════════════════════════════════════════════════════════════════

NeoNode* NeoParser::parseTryExcept() {
    int tl = current().line, tc = current().col;
    advance(); // consume 'try'

    expect(NeoTokType::COLON, "try statement");
    match(NeoTokType::NEWLINE);

    TryExceptNode* node = _arena.make<TryExceptNode>(tl, tc);
    if (!node) return nullptr;

    parseBlock(node->try_body);

    skipNewlines();
    if (!check(NeoTokType::EXCEPT)) {
        recordError("expected 'except' after try block", current().line, current().col);
        return node;
    }
    advance(); // consume 'except'

    // Optional: skip exception type identifier (e.g., 'Error', 'Exception')
    if (check(NeoTokType::IDENTIFIER)) {
        // Check if next is 'as' — if not, this is the variable name
        if (peek(1).type == NeoTokType::AS) {
            advance(); // skip exception type name
        }
    }

    // Optional: 'as varname'
    if (check(NeoTokType::AS)) {
        advance(); // consume 'as'
        if (check(NeoTokType::IDENTIFIER)) {
            node->except_var = current().value;
            advance();
        }
    } else if (check(NeoTokType::IDENTIFIER)) {
        // Could also be: except e (without 'as')
        node->except_var = current().value;
        advance();
    }

    expect(NeoTokType::COLON, "except clause");
    match(NeoTokType::NEWLINE);
    parseBlock(node->except_body);

    return node;
}

// ═════════════════════════════════════════════════════════════════
// parseAssignmentOrExpr
// ═════════════════════════════════════════════════════════════════

NeoNode* NeoParser::parseAssignmentOrExpr() {
    // Look-ahead: if IDENTIFIER followed by = or :=, it's an assignment.
    // Otherwise parse as expression (which may itself be a bare identifier).
    if (check(NeoTokType::IDENTIFIER)) {
        NeoTokType next = peek(1).type;
        if (next == NeoTokType::ASSIGN || next == NeoTokType::DELAYED_ASSIGN) {
            int tl = current().line, tc = current().col;
            std::string target = current().value;
            advance(); // consume IDENTIFIER
            bool delayed = (current().type == NeoTokType::DELAYED_ASSIGN);
            advance(); // consume = or :=
            NeoNode* val = parseExpression();
            if (!val) { syncToNextStatement(); return nullptr; }
            match(NeoTokType::NEWLINE);
            return _arena.make<AssignmentNode>(target, val, delayed, tl, tc);
        }
    }

    // Expression statement
    NeoNode* expr = parseExpression();
    match(NeoTokType::NEWLINE);
    return expr;
}

// ═════════════════════════════════════════════════════════════════
// Pratt expression parser
// ═════════════════════════════════════════════════════════════════

/**
 * Precedence table (higher number = tighter binding):
 *   or          1
 *   and         2
 *   == != < > <= >= 3
 *   + -         4
 *   * /         5
 *   ^ **        6  (right-assoc)
 */
int NeoParser::infixPrecedence(const NeoToken& t) const {
    switch (t.type) {
        case NeoTokType::OR:        return 1;
        case NeoTokType::AND:       return 2;
        case NeoTokType::EQEQ:
        case NeoTokType::NEQ:
        case NeoTokType::LT:
        case NeoTokType::LE:
        case NeoTokType::GT:
        case NeoTokType::GE:        return 3;
        case NeoTokType::PIPE:      return 3;   ///< bitwise OR  (same level as comparison)
        case NeoTokType::CARETCARET: return 4;  ///< bitwise XOR
        case NeoTokType::AMP:       return 5;   ///< bitwise AND
        case NeoTokType::PLUS:
        case NeoTokType::MINUS:     return 6;
        case NeoTokType::LSHIFT:
        case NeoTokType::RSHIFT:    return 7;   ///< shifts
        case NeoTokType::STAR:
        case NeoTokType::SLASH:     return 8;
        case NeoTokType::CARET:
        case NeoTokType::STARSTAR:  return 9;
        default:                    return -1;
    }
}

bool NeoParser::isRightAssoc(const NeoToken& t) const {
    return t.type == NeoTokType::CARET || t.type == NeoTokType::STARSTAR;
}

BinaryOpNode::OpKind NeoParser::tokenToBinOp(const NeoToken& t) const {
    switch (t.type) {
        case NeoTokType::PLUS:       return BinaryOpNode::OpKind::Add;
        case NeoTokType::MINUS:      return BinaryOpNode::OpKind::Sub;
        case NeoTokType::STAR:       return BinaryOpNode::OpKind::Mul;
        case NeoTokType::SLASH:      return BinaryOpNode::OpKind::Div;
        case NeoTokType::CARET:
        case NeoTokType::STARSTAR:   return BinaryOpNode::OpKind::Pow;
        case NeoTokType::EQEQ:       return BinaryOpNode::OpKind::Eq;
        case NeoTokType::NEQ:        return BinaryOpNode::OpKind::Ne;
        case NeoTokType::LT:         return BinaryOpNode::OpKind::Lt;
        case NeoTokType::LE:         return BinaryOpNode::OpKind::Le;
        case NeoTokType::GT:         return BinaryOpNode::OpKind::Gt;
        case NeoTokType::GE:         return BinaryOpNode::OpKind::Ge;
        case NeoTokType::AND:        return BinaryOpNode::OpKind::And;
        case NeoTokType::OR:         return BinaryOpNode::OpKind::Or;
        case NeoTokType::AMP:        return BinaryOpNode::OpKind::BitAnd;
        case NeoTokType::PIPE:       return BinaryOpNode::OpKind::BitOr;
        case NeoTokType::CARETCARET: return BinaryOpNode::OpKind::BitXor;
        case NeoTokType::LSHIFT:     return BinaryOpNode::OpKind::LShift;
        case NeoTokType::RSHIFT:     return BinaryOpNode::OpKind::RShift;
        default:
            return BinaryOpNode::OpKind::Add;
    }
}

NeoNode* NeoParser::parseExpression(int minPrec) {
    NeoNode* left = parseUnary();
    if (!left) return nullptr;

    while (true) {
        const NeoToken& op = current();
        int prec = infixPrecedence(op);
        if (prec < 0 || prec < minPrec) break;

        int tl = op.line, tc = op.col;
        BinaryOpNode::OpKind kind = tokenToBinOp(op);
        advance(); // consume operator

        // Right side: for right-assoc use same precedence, else prec+1
        int nextPrec = isRightAssoc(op) ? prec : prec + 1;
        NeoNode* right = parseExpression(nextPrec);
        if (!right) {
            recordError("expected right-hand side of operator", tl, tc);
            return left;
        }

        NeoNode* node = _arena.make<BinaryOpNode>(kind, left, right, tl, tc);
        if (!node) return left;
        left = node;
    }
    return left;
}

// ═════════════════════════════════════════════════════════════════
// parseUnary
// ═════════════════════════════════════════════════════════════════

NeoNode* NeoParser::parseUnary() {
    // Unary minus
    if (check(NeoTokType::MINUS)) {
        int tl = current().line, tc = current().col;
        advance();
        NeoNode* operand = parseUnary(); // right-recursive but bounded depth
        if (!operand) return nullptr;
        return _arena.make<UnaryOpNode>(UnaryOpNode::OpKind::Neg, operand, tl, tc);
    }
    // Logical not
    if (check(NeoTokType::NOT)) {
        int tl = current().line, tc = current().col;
        advance();
        NeoNode* operand = parseUnary();
        if (!operand) return nullptr;
        return _arena.make<UnaryOpNode>(UnaryOpNode::OpKind::Not, operand, tl, tc);
    }
    // Bitwise not ~ (Phase 6)
    if (check(NeoTokType::TILDE)) {
        int tl = current().line, tc = current().col;
        advance();
        NeoNode* operand = parseUnary();
        if (!operand) return nullptr;
        return _arena.make<UnaryOpNode>(UnaryOpNode::OpKind::BitNot, operand, tl, tc);
    }
    return parsePrimary();
}

// ═════════════════════════════════════════════════════════════════
// parseIndexOp — target[idx] or target[row, col]
// ═════════════════════════════════════════════════════════════════

NeoNode* NeoParser::parseIndexOp(NeoNode* target, int line, int col) {
    IndexOpNode* node = _arena.make<IndexOpNode>(target, line, col);
    if (!node) return target;

    // Consume the opening '[' (may loop for chained indexing)
    while (check(NeoTokType::LBRACKET)) {
        advance(); // consume [
        // Parse comma-separated index expressions
        while (!check(NeoTokType::RBRACKET) && !atEnd()) {
            NeoNode* idx = parseExpression();
            if (idx) node->indices.push_back(idx);
            if (!match(NeoTokType::COMMA)) break;
        }
        expect(NeoTokType::RBRACKET, "index expression");
        // Allow only one bracket pair (chained: L[0][1] would need two nodes)
        break;
    }
    return node;
}

// ═════════════════════════════════════════════════════════════════
// parsePrimary
// ═════════════════════════════════════════════════════════════════

NeoNode* NeoParser::parsePrimary() {
    const NeoToken& tok = current();

    // ── Number literal ────────────────────────────────────────────
    if (tok.type == NeoTokType::NUMBER) {
        advance();
        double v = std::strtod(tok.value.c_str(), nullptr);
        NumberNode* node = _arena.make<NumberNode>(v, tok.value, tok.line, tok.col);
        if (!node) return nullptr;
        // Fill exact rational if value is an integer
        int64_t iv = static_cast<int64_t>(v);
        if (static_cast<double>(iv) == v) { node->num = iv; node->den = 1; }
        return node;
    }

    // ── True / False / None literals (treated as Numbers/Symbols) ─
    if (tok.type == NeoTokType::TRUE_KW) {
        advance();
        NumberNode* node = _arena.make<NumberNode>(1.0, "true", tok.line, tok.col);
        if (node) { node->num = 1; node->den = 1; }
        return node;
    }
    if (tok.type == NeoTokType::FALSE_KW) {
        advance();
        NumberNode* node = _arena.make<NumberNode>(0.0, "false", tok.line, tok.col);
        if (node) { node->num = 0; node->den = 1; }
        return node;
    }
    if (tok.type == NeoTokType::NONE_KW) {
        advance();
        return _arena.make<SymbolNode>("None", tok.line, tok.col);
    }

    // ── String literal ────────────────────────────────────────────
    if (tok.type == NeoTokType::STRING) {
        advance();
        return _arena.make<StringNode>(tok.value, tok.line, tok.col);
    }

    // ── Identifier: variable or function call ─────────────────────
    if (tok.type == NeoTokType::IDENTIFIER) {
        advance();
        // Function call: identifier immediately followed by (
        if (check(NeoTokType::LPAREN)) {
            FunctionCallNode* call = _arena.make<FunctionCallNode>(tok.value, tok.line, tok.col);
            if (!call) return nullptr;
            advance(); // consume (
            while (!check(NeoTokType::RPAREN) && !atEnd()) {
                NeoNode* arg = parseExpression();
                if (arg) call->args.push_back(arg);
                if (!match(NeoTokType::COMMA)) break;
            }
            expect(NeoTokType::RPAREN, "function call argument list");
            // Check for chained indexing: f(x)[0]
            if (check(NeoTokType::LBRACKET)) {
                return parseIndexOp(call, tok.line, tok.col);
            }
            return call;
        }
        // Subscript: variable followed by [
        if (check(NeoTokType::LBRACKET)) {
            NeoNode* varNode = _arena.make<SymbolNode>(tok.value, tok.line, tok.col);
            return parseIndexOp(varNode, tok.line, tok.col);
        }
        // Variable reference
        return _arena.make<SymbolNode>(tok.value, tok.line, tok.col);
    }

    // ── Grouped expression: ( expr ) ─────────────────────────────
    if (tok.type == NeoTokType::LPAREN) {
        advance(); // consume (
        NeoNode* expr = parseExpression();
        expect(NeoTokType::RPAREN, "grouped expression");
        return expr;
    }

    // ── List literal: [ expr, ... ] ───────────────────────────────
    if (tok.type == NeoTokType::LBRACKET) {
        int tl = tok.line, tc = tok.col;
        advance(); // consume [
        FunctionCallNode* listNode = _arena.make<FunctionCallNode>("__list__", tl, tc);
        if (!listNode) return nullptr;
        while (!check(NeoTokType::RBRACKET) && !atEnd()) {
            NeoNode* elem = parseExpression();
            if (elem) listNode->args.push_back(elem);
            if (!match(NeoTokType::COMMA)) break;
        }
        expect(NeoTokType::RBRACKET, "list literal");
        // Check for immediate indexing: [1,2,3][0]
        if (check(NeoTokType::LBRACKET)) {
            return parseIndexOp(listNode, tl, tc);
        }
        return listNode;
    }

    // ── Dictionary literal: { "key": expr, ... } ─────────────────
    if (tok.type == NeoTokType::LBRACE) {
        int tl = tok.line, tc = tok.col;
        advance(); // consume {
        DictLiteralNode* dictNode = _arena.make<DictLiteralNode>(tl, tc);
        if (!dictNode) return nullptr;
        while (!check(NeoTokType::RBRACE) && !atEnd()) {
            // Parse key: either string literal or identifier
            std::string key;
            if (check(NeoTokType::STRING)) {
                key = current().value;
                advance();
            } else if (check(NeoTokType::IDENTIFIER)) {
                key = current().value;
                advance();
            } else {
                char buf[128];
                std::snprintf(buf, sizeof(buf),
                    "dictionary key must be a string or identifier at line %d",
                    current().line);
                recordError(buf, current().line, current().col);
                syncToNextStatement();
                return nullptr;
            }
            expect(NeoTokType::COLON, "dictionary key-value pair");
            NeoNode* val = parseExpression();
            if (!val) { syncToNextStatement(); return nullptr; }
            dictNode->keys.push_back(key);
            dictNode->values.push_back(val);
            if (!match(NeoTokType::COMMA)) break;
        }
        expect(NeoTokType::RBRACE, "dictionary literal");
        // Allow immediate indexing: {"a": 1}["a"]
        if (check(NeoTokType::LBRACKET)) {
            return parseIndexOp(dictNode, tl, tc);
        }
        return dictNode;
    }

    // ── Unexpected token ─────────────────────────────────────────
    char buf[128];
    std::snprintf(buf, sizeof(buf),
        "unexpected token '%s' at line %d col %d",
        tok.value.c_str(), tok.line, tok.col);
    recordError(buf, tok.line, tok.col);
    syncToNextStatement();
    return nullptr;
}

// ═════════════════════════════════════════════════════════════════
// parseImport — import X [as Y]  or  from X import a, b, c
// (Phase 8: Module System)
// ═════════════════════════════════════════════════════════════════

NeoNode* NeoParser::parseImport() {
    int tl = current().line, tc = current().col;

    // ── from X import a, b, c ─────────────────────────────────────
    if (check(NeoTokType::FROM)) {
        advance(); // consume 'from'

        if (!check(NeoTokType::IDENTIFIER)) {
            recordError("expected module name after 'from'", current().line, current().col);
            syncToNextStatement();
            return nullptr;
        }
        std::string modName = current().value;
        advance(); // consume module name

        if (!check(NeoTokType::IMPORT)) {
            recordError("expected 'import' after module name in 'from' statement",
                        current().line, current().col);
            syncToNextStatement();
            return nullptr;
        }
        advance(); // consume 'import'

        ImportNode* node = _arena.make<ImportNode>(modName, tl, tc);
        if (!node) return nullptr;
        node->is_from = true;

        // Parse comma-separated name list
        while (!atEnd() && !check(NeoTokType::NEWLINE) && !check(NeoTokType::END_OF_FILE)) {
            if (!check(NeoTokType::IDENTIFIER)) {
                recordError("expected identifier in import list", current().line, current().col);
                break;
            }
            node->names.push_back(current().value);
            advance();
            if (!match(NeoTokType::COMMA)) break;
        }
        match(NeoTokType::NEWLINE);
        return node;
    }

    // ── import X [as Y] ──────────────────────────────────────────
    advance(); // consume 'import'

    if (!check(NeoTokType::IDENTIFIER)) {
        recordError("expected module name after 'import'", current().line, current().col);
        syncToNextStatement();
        return nullptr;
    }
    std::string modName = current().value;
    advance(); // consume module name

    ImportNode* node = _arena.make<ImportNode>(modName, tl, tc);
    if (!node) return nullptr;
    node->is_from = false;

    // Optional: 'as alias'
    if (check(NeoTokType::AS)) {
        advance(); // consume 'as'
        if (check(NeoTokType::IDENTIFIER)) {
            node->alias = current().value;
            advance();
        }
    }
    match(NeoTokType::NEWLINE);
    return node;
}
