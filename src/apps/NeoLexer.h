/**
 * NeoLexer.h — Tokenizer for the NeoLanguage.
 *
 * Produces a flat token stream from NeoLanguage source code.
 *
 * Features:
 *   · Python-style significant indentation (INDENT / DEDENT tokens)
 *   · All keyword, operator, and punctuation token types
 *   · := recognised as a single DELAYED_ASSIGN token
 *   · ** recognised as power operator (alias for ^)
 *   · -> and @ for future lambda / decorator syntax
 *   · String literals with escape sequences (\n \t \\ \")
 *   · Single-line comments (#...)
 *   · Accurate line/column tracking
 *   · ERROR tokens with descriptive messages for invalid input
 *
 * Memory: the returned token vector uses PSRAMAllocator<NeoToken>.
 *
 * All types are in the `neo` namespace to avoid conflicts with the
 * existing math engine Tokenizer / Token / TokenType types.
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage frontend
 */
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "../math/cas/PSRAMAllocator.h"

// ════════════════════════════════════════════════════════════════════
// NeoLanguage types live in namespace neo to avoid ODR conflicts with
// the existing math::Tokenizer (which also defines Token / TokenType).
// ════════════════════════════════════════════════════════════════════

namespace neo {

// ════════════════════════════════════════════════════════════════════
// NeoTokType — every distinct lexical category
// ════════════════════════════════════════════════════════════════════

enum class NeoTokType : uint8_t {
    // ── Keywords ─────────────────────────────────────────────────
    DEF,
    IF,
    ELSE,
    ELIF,
    WHILE,
    FOR,
    IN,
    RETURN,
    AND,
    OR,
    NOT,
    TRUE_KW,    ///< 'true' / 'True'
    FALSE_KW,   ///< 'false' / 'False'
    NONE_KW,    ///< 'None' / 'none'
    TRY,        ///< 'try'
    EXCEPT,     ///< 'except'
    AS,         ///< 'as'
    IMPORT,     ///< 'import'
    FROM,       ///< 'from'

    // ── Literals ─────────────────────────────────────────────────
    NUMBER,
    STRING,
    IDENTIFIER,

    // ── Operators ────────────────────────────────────────────────
    PLUS,       ///< +
    MINUS,      ///< -
    STAR,       ///< *
    SLASH,      ///< /
    CARET,      ///< ^
    ASSIGN,     ///< =
    DELAYED_ASSIGN, ///< :=
    EQEQ,       ///< ==
    NEQ,        ///< !=
    LT,         ///< <
    LE,         ///< <=
    GT,         ///< >
    GE,         ///< >=
    STARSTAR,   ///< **  (power, synonym for ^)
    ARROW,      ///< ->
    AT,         ///< @

    // ── Bitwise operators ─────────────────────────────────────────
    AMP,        ///< &   (bitwise AND)
    PIPE,       ///< |   (bitwise OR)
    TILDE,      ///< ~   (bitwise NOT, unary)
    LSHIFT,     ///< <<  (left shift)
    RSHIFT,     ///< >>  (right shift)
    CARETCARET, ///< ^^  (bitwise XOR)

    // ── Punctuation ──────────────────────────────────────────────
    LPAREN,     ///< (
    RPAREN,     ///< )
    LBRACKET,   ///< [
    RBRACKET,   ///< ]
    LBRACE,     ///< {
    RBRACE,     ///< }
    COMMA,      ///< ,
    COLON,      ///< :
    DOT,        ///< .
    SEMICOLON,  ///< ;

    // ── Structural ───────────────────────────────────────────────
    INDENT,
    DEDENT,
    NEWLINE,
    END_OF_FILE,

    // ── Error ────────────────────────────────────────────────────
    ERROR,
};

// ════════════════════════════════════════════════════════════════════
// NeoToken — a single lexical unit with source position
// ════════════════════════════════════════════════════════════════════

struct NeoToken {
    NeoTokType  type;
    std::string value; ///< Raw source text (or error message for ERROR)
    int         line;
    int         col;

    NeoToken(NeoTokType t, std::string v, int l, int c)
        : type(t), value(std::move(v)), line(l), col(c) {}
};

// ════════════════════════════════════════════════════════════════════
// NeoLexer — state-machine driven tokenizer
// ════════════════════════════════════════════════════════════════════

/**
 * Usage:
 *   neo::NeoLexer lex(sourceCode);
 *   auto tokens = lex.tokenize();
 *   if (!lex.lastError().empty()) { // handle error
 *   }
 *
 * tokenize() is idempotent — calling it multiple times re-tokenizes
 * from scratch (position is reset to 0).
 */
class NeoLexer {
public:
    using TokenVec = std::vector<NeoToken, cas::PSRAMAllocator<NeoToken>>;

    explicit NeoLexer(const std::string& source);

    /**
     * Run the lexer and return the complete token stream.
     * The last token is always END_OF_FILE.
     * ERROR tokens may appear inline for invalid input — the lexer
     * continues rather than aborting on the first error.
     */
    TokenVec tokenize();

    /**
     * Returns a description of the last error encountered, or ""
     * if tokenize() completed without errors.
     */
    const std::string& lastError() const { return _lastError; }

private:
    // ── Source state ─────────────────────────────────────────────
    std::string        _src;   ///< Owned copy — safe against temporary args
    size_t             _pos;
    int                _line;
    int                _col;

    // ── Indent stack ─────────────────────────────────────────────
    // Stores the column-width of each open indent level.
    std::vector<int, cas::PSRAMAllocator<int>> _indentStack;

    // ── Error state ──────────────────────────────────────────────
    std::string _lastError;

    // ── Token construction helpers ───────────────────────────────
    NeoToken makeToken(NeoTokType t, const std::string& val) const;
    NeoToken makeToken(NeoTokType t, char c) const;
    NeoToken errorToken(const std::string& msg) const;

    // ── Character utilities ───────────────────────────────────────
    char  peek(int offset = 0) const;
    char  advance();
    bool  atEnd() const;
    bool  isDigit(char c) const { return c >= '0' && c <= '9'; }
    bool  isAlpha(char c) const { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
    bool  isAlNum(char c) const { return isAlpha(c) || isDigit(c); }

    // ── Sub-lexers ────────────────────────────────────────────────
    NeoToken lexNumber();
    NeoToken lexString(char quote);
    NeoToken lexIdentifierOrKeyword();
    NeoToken lexOperatorOrPunct();

    // ── Indentation helpers ───────────────────────────────────────
    /**
     * Called at the start of a (non-blank, non-comment) physical line.
     * Counts leading spaces/tabs, then emits INDENT or DEDENT tokens
     * into `out` as needed.  Returns the column of the first real char.
     */
    void handleIndent(TokenVec& out, int newIndent);

    /**
     * Count leading whitespace on the current line starting at `_pos`.
     * Tabs are counted as single spaces (caller may expand if needed).
     * Does NOT advance _pos.
     */
    int  countIndent() const;

    /**
     * Skip a comment to end of line (does not consume the newline).
     */
    void skipComment();

    /**
     * Map an identifier string to its keyword NeoTokType, or
     * IDENTIFIER if it is not a keyword.
     */
    static NeoTokType keywordType(const std::string& id);
};

} // namespace neo

// ════════════════════════════════════════════════════════════════════
// Convenience aliases at global scope for callers that don't
// want to qualify every use with `neo::`.
// ════════════════════════════════════════════════════════════════════
using NeoTokType = neo::NeoTokType;
using NeoToken   = neo::NeoToken;
// NeoLexer itself is used as neo::NeoLexer to avoid ambiguity.
