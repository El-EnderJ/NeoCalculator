/**
 * NeoLexer.cpp — Tokenizer implementation for NeoLanguage.
 *
 * State-machine tokenizer with Python-style significant indentation.
 * Iterative (no recursion) to avoid ESP32 stack overflow.
 *
 * Indentation rules (simplified Python model):
 *   · At the start of each logical line count leading spaces.
 *   · Blank lines and comment-only lines do NOT affect indentation.
 *   · If indent > top-of-stack → emit INDENT, push new level.
 *   · If indent < top-of-stack → emit DEDENT(s) until stack matches.
 *   · If indent == top-of-stack → no structural token.
 *   · LIMITATION: tabs count as 1 space. Mixing tabs and spaces can
 *     cause indentation mismatches.  Users should use spaces only.
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage frontend
 */

#include "NeoLexer.h"
#include <cstdio>
#include <cstring>
#include <cctype>

// ── Constructor ───────────────────────────────────────────────────

neo::NeoLexer::NeoLexer(const std::string& source)
    : _src(source)   // copy — safe against temporaries
    , _pos(0), _line(1), _col(1)
{
    // Seed the indent stack with column-0 (global scope)
    _indentStack.push_back(0);
}

// ═════════════════════════════════════════════════════════════════
// tokenize() — main entry point
// ═════════════════════════════════════════════════════════════════

neo::NeoLexer::TokenVec neo::NeoLexer::tokenize() {
    // Reset state so repeated calls re-tokenize from scratch
    _pos  = 0;
    _line = 1;
    _col  = 1;
    _lastError.clear();
    _indentStack.clear();
    _indentStack.push_back(0);

    TokenVec tokens;
    bool atLineStart = true; // Are we at the first non-whitespace of a line?

    while (!atEnd()) {
        // ── Handle line-start indentation ───────────────────────
        if (atLineStart) {
            atLineStart = false;

            // Count leading whitespace
            int indent = countIndent();

            // Skip past whitespace
            while (_pos < _src.size() && (_src[_pos] == ' ' || _src[_pos] == '\t')) {
                advance();
            }

            // Skip blank lines and comment-only lines — they don't
            // affect indentation state or emit any structural tokens.
            if (atEnd() || _src[_pos] == '\n' || _src[_pos] == '\r' || _src[_pos] == '#') {
                if (!atEnd() && _src[_pos] == '#') skipComment();
                if (!atEnd()) {
                    // Consume the newline
                    if (_src[_pos] == '\r' && _pos + 1 < _src.size() && _src[_pos+1] == '\n') advance();
                    advance(); // \n
                    _line++;
                    _col = 1;
                }
                atLineStart = true;
                continue;
            }

            // Real content line — process indentation
            handleIndent(tokens, indent);
        }

        if (atEnd()) break;

        char c = peek();

        // ── Skip spaces / tabs within a line ────────────────────
        if (c == ' ' || c == '\t') {
            advance();
            continue;
        }

        // ── Comment ──────────────────────────────────────────────
        if (c == '#') {
            skipComment();
            continue;
        }

        // ── Newline ───────────────────────────────────────────────
        if (c == '\n' || c == '\r') {
            int tl = _line, tc = _col;
            if (c == '\r' && _pos + 1 < _src.size() && _src[_pos+1] == '\n') advance();
            advance();
            tokens.emplace_back(NeoTokType::NEWLINE, "\n", tl, tc);
            _line++;
            _col = 1;
            atLineStart = true;
            continue;
        }

        // ── Number ────────────────────────────────────────────────
        if (isDigit(c) || (c == '.' && isDigit(peek(1)))) {
            tokens.push_back(lexNumber());
            continue;
        }

        // ── String literal ────────────────────────────────────────
        if (c == '"' || c == '\'') {
            advance(); // consume opening quote
            tokens.push_back(lexString(c));
            continue;
        }

        // ── Identifier or keyword ─────────────────────────────────
        if (isAlpha(c)) {
            tokens.push_back(lexIdentifierOrKeyword());
            continue;
        }

        // ── Operators and punctuation ─────────────────────────────
        tokens.push_back(lexOperatorOrPunct());
    }

    // ── Emit final NEWLINE if last line had no newline ────────────
    if (!tokens.empty() && tokens.back().type != NeoTokType::NEWLINE)
        tokens.emplace_back(NeoTokType::NEWLINE, "\n", _line, _col);

    // ── Close any open indent levels ─────────────────────────────
    while (_indentStack.size() > 1) {
        _indentStack.pop_back();
        tokens.emplace_back(NeoTokType::DEDENT, "", _line, _col);
    }

    tokens.emplace_back(NeoTokType::END_OF_FILE, "", _line, _col);
    return tokens;
}

// ═════════════════════════════════════════════════════════════════
// handleIndent — emit INDENT / DEDENT tokens
// ═════════════════════════════════════════════════════════════════

void neo::NeoLexer::handleIndent(TokenVec& out, int newIndent) {
    int current = _indentStack.back();

    if (newIndent > current) {
        // Deeper level — one INDENT token
        _indentStack.push_back(newIndent);
        out.emplace_back(NeoTokType::INDENT, "", _line, 1);
    } else if (newIndent < current) {
        // Shallower — pop until we match (or error)
        while (_indentStack.size() > 1 && _indentStack.back() > newIndent) {
            _indentStack.pop_back();
            out.emplace_back(NeoTokType::DEDENT, "", _line, 1);
        }
        if (_indentStack.back() != newIndent) {
            // Mismatched dedent
            std::string msg = "IndentationError: unindent does not match any outer indentation level (line ";
            msg += std::to_string(_line);
            msg += ")";
            _lastError = msg;
            out.emplace_back(NeoTokType::ERROR, msg, _line, 1);
        }
    }
    // Equal — no structural token needed
}

// ═════════════════════════════════════════════════════════════════
// countIndent — measure leading whitespace without advancing
// ═════════════════════════════════════════════════════════════════

int neo::NeoLexer::countIndent() const {
    int n = 0;
    size_t p = _pos;
    while (p < _src.size() && (_src[p] == ' ' || _src[p] == '\t')) {
        n++;
        p++;
    }
    return n;
}

// ═════════════════════════════════════════════════════════════════
// skipComment — consume from # to end of line (not the newline)
// ═════════════════════════════════════════════════════════════════

void neo::NeoLexer::skipComment() {
    while (!atEnd() && _src[_pos] != '\n' && _src[_pos] != '\r') {
        advance();
    }
}

// ═════════════════════════════════════════════════════════════════
// lexNumber — integer, float, scientific notation
// ═════════════════════════════════════════════════════════════════

NeoToken neo::NeoLexer::lexNumber() {
    int tl = _line, tc = _col;
    std::string raw;

    // Check for binary (0b) or hexadecimal (0x) prefix
    if (peek() == '0' && !atEnd()) {
        char next = peek(1);
        if (next == 'b' || next == 'B') {
            // Binary literal: 0b1010
            raw += advance(); // '0'
            raw += advance(); // 'b' or 'B'
            if (atEnd() || !(peek() == '0' || peek() == '1')) {
                std::string msg = "invalid binary literal: '" + raw + "'";
                _lastError = msg;
                return NeoToken(NeoTokType::ERROR, msg, tl, tc);
            }
            std::string digits;
            while (!atEnd() && (peek() == '0' || peek() == '1')) {
                digits += advance();
                raw    += digits.back();
            }
            // Convert binary string to decimal string
            uint64_t val = 0;
            for (char d : digits) val = (val << 1) | (d - '0');
            // Return token with decimal representation; store original raw
            std::string dec = std::to_string(val);
            return NeoToken(NeoTokType::NUMBER, dec, tl, tc);
        }
        if (next == 'x' || next == 'X') {
            // Hex literal: 0xFF
            raw += advance(); // '0'
            raw += advance(); // 'x' or 'X'
            if (atEnd() || !std::isxdigit(static_cast<unsigned char>(peek()))) {
                std::string msg = "invalid hex literal: '" + raw + "'";
                _lastError = msg;
                return NeoToken(NeoTokType::ERROR, msg, tl, tc);
            }
            std::string hexDigits;
            while (!atEnd() && std::isxdigit(static_cast<unsigned char>(peek()))) {
                hexDigits += advance();
                raw       += hexDigits.back();
            }
            // Convert hex to decimal via strtoul
            uint64_t val = 0;
            for (char d : hexDigits) {
                val <<= 4;
                if (d >= '0' && d <= '9') val |= (uint64_t)(d - '0');
                else if (d >= 'a' && d <= 'f') val |= (uint64_t)(d - 'a' + 10);
                else if (d >= 'A' && d <= 'F') val |= (uint64_t)(d - 'A' + 10);
            }
            std::string dec = std::to_string(val);
            return NeoToken(NeoTokType::NUMBER, dec, tl, tc);
        }
    }

    // Standard integer part
    while (!atEnd() && isDigit(peek())) raw += advance();

    // Decimal part
    if (!atEnd() && peek() == '.' && isDigit(peek(1))) {
        raw += advance(); // '.'
        while (!atEnd() && isDigit(peek())) raw += advance();
    }

    // Scientific notation: e / E followed by optional sign and digits
    if (!atEnd() && (peek() == 'e' || peek() == 'E')) {
        raw += advance(); // 'e' or 'E'
        if (!atEnd() && (peek() == '+' || peek() == '-')) raw += advance();
        if (!atEnd() && isDigit(peek())) {
            while (!atEnd() && isDigit(peek())) raw += advance();
        } else {
            // Malformed exponent — emit as error token
            std::string msg = "invalid number literal: '" + raw + "'";
            _lastError = msg;
            return NeoToken(NeoTokType::ERROR, msg, tl, tc);
        }
    }

    return NeoToken(NeoTokType::NUMBER, raw, tl, tc);
}

// ═════════════════════════════════════════════════════════════════
// lexString — string literal (quote already consumed)
// ═════════════════════════════════════════════════════════════════

NeoToken neo::NeoLexer::lexString(char quote) {
    int tl = _line, tc = _col - 1; // opening quote position
    std::string val;

    while (!atEnd() && peek() != quote) {
        char c = advance();
        if (c == '\n' || c == '\r') {
            // Unterminated string
            std::string msg = "unterminated string literal (line ";
            msg += std::to_string(tl); msg += ")";
            _lastError = msg;
            return NeoToken(NeoTokType::ERROR, msg, tl, tc);
        }
        if (c == '\\') {
            if (atEnd()) break;
            char esc = advance();
            switch (esc) {
                case 'n':  val += '\n'; break;
                case 't':  val += '\t'; break;
                case 'r':  val += '\r'; break;
                case '\\': val += '\\'; break;
                case '\'': val += '\''; break;
                case '"':  val += '"';  break;
                case '0':  val += '\0'; break;
                default:
                    // Unknown escape — pass both chars through
                    val += '\\';
                    val += esc;
                    break;
            }
        } else {
            val += c;
        }
    }

    if (atEnd()) {
        std::string msg = "unterminated string literal (line ";
        msg += std::to_string(tl); msg += ")";
        _lastError = msg;
        return NeoToken(NeoTokType::ERROR, msg, tl, tc);
    }
    advance(); // closing quote
    return NeoToken(NeoTokType::STRING, val, tl, tc);
}

// ═════════════════════════════════════════════════════════════════
// lexIdentifierOrKeyword
// ═════════════════════════════════════════════════════════════════

NeoToken neo::NeoLexer::lexIdentifierOrKeyword() {
    int tl = _line, tc = _col;
    std::string name;
    while (!atEnd() && isAlNum(peek())) name += advance();

    NeoTokType tt = keywordType(name);
    return NeoToken(tt, name, tl, tc);
}

// ═════════════════════════════════════════════════════════════════
// lexOperatorOrPunct
// ═════════════════════════════════════════════════════════════════

NeoToken neo::NeoLexer::lexOperatorOrPunct() {
    int tl = _line, tc = _col;
    char c = advance();

    switch (c) {
        case '+': return NeoToken(NeoTokType::PLUS,      "+",  tl, tc);
        case '/': return NeoToken(NeoTokType::SLASH,     "/",  tl, tc);
        case '(': return NeoToken(NeoTokType::LPAREN,    "(",  tl, tc);
        case ')': return NeoToken(NeoTokType::RPAREN,    ")",  tl, tc);
        case '[': return NeoToken(NeoTokType::LBRACKET,  "[",  tl, tc);
        case ']': return NeoToken(NeoTokType::RBRACKET,  "]",  tl, tc);
        case '{': return NeoToken(NeoTokType::LBRACE,    "{",  tl, tc);
        case '}': return NeoToken(NeoTokType::RBRACE,    "}",  tl, tc);
        case ',': return NeoToken(NeoTokType::COMMA,     ",",  tl, tc);
        case '.': return NeoToken(NeoTokType::DOT,       ".",  tl, tc);
        case ';': return NeoToken(NeoTokType::SEMICOLON, ";",  tl, tc);
        case '@': return NeoToken(NeoTokType::AT,        "@",  tl, tc);

        // ── Bitwise operators ─────────────────────────────────────
        case '&': return NeoToken(NeoTokType::AMP,   "&",  tl, tc);
        case '|': return NeoToken(NeoTokType::PIPE,  "|",  tl, tc);
        case '~': return NeoToken(NeoTokType::TILDE, "~",  tl, tc);

        case '*':
            // ** or *
            if (!atEnd() && peek() == '*') { advance(); return NeoToken(NeoTokType::STARSTAR, "**", tl, tc); }
            return NeoToken(NeoTokType::STAR, "*", tl, tc);

        case '-':
            // -> or -
            if (!atEnd() && peek() == '>') { advance(); return NeoToken(NeoTokType::ARROW, "->", tl, tc); }
            return NeoToken(NeoTokType::MINUS, "-", tl, tc);

        case '^':
            // ^^ (XOR) or ^ (power)
            if (!atEnd() && peek() == '^') { advance(); return NeoToken(NeoTokType::CARETCARET, "^^", tl, tc); }
            return NeoToken(NeoTokType::CARET, "^", tl, tc);

        case '=':
            // == or =
            if (!atEnd() && peek() == '=') { advance(); return NeoToken(NeoTokType::EQEQ, "==", tl, tc); }
            return NeoToken(NeoTokType::ASSIGN, "=", tl, tc);

        case '!':
            // !=
            if (!atEnd() && peek() == '=') { advance(); return NeoToken(NeoTokType::NEQ, "!=", tl, tc); }
            {
                std::string msg = "unexpected character '!'";
                _lastError = msg;
                return NeoToken(NeoTokType::ERROR, msg, tl, tc);
            }

        case '<':
            // <=  or  <<  or  <
            if (!atEnd() && peek() == '=') { advance(); return NeoToken(NeoTokType::LE, "<=", tl, tc); }
            if (!atEnd() && peek() == '<') { advance(); return NeoToken(NeoTokType::LSHIFT, "<<", tl, tc); }
            return NeoToken(NeoTokType::LT, "<", tl, tc);

        case '>':
            // >= or >> or >
            if (!atEnd() && peek() == '=') { advance(); return NeoToken(NeoTokType::GE, ">=", tl, tc); }
            if (!atEnd() && peek() == '>') { advance(); return NeoToken(NeoTokType::RSHIFT, ">>", tl, tc); }
            return NeoToken(NeoTokType::GT, ">", tl, tc);

        case ':':
            // := or :
            if (!atEnd() && peek() == '=') { advance(); return NeoToken(NeoTokType::DELAYED_ASSIGN, ":=", tl, tc); }
            return NeoToken(NeoTokType::COLON, ":", tl, tc);

        default: {
            std::string msg = "unexpected character '";
            msg += c;
            msg += "'";
            _lastError = msg;
            return NeoToken(NeoTokType::ERROR, msg, tl, tc);
        }
    }
}

// ═════════════════════════════════════════════════════════════════
// keywordType — map identifier string → TokenType
// ═════════════════════════════════════════════════════════════════

NeoTokType neo::NeoLexer::keywordType(const std::string& id) {
    // Case-sensitive keyword table
    if (id == "def")    return NeoTokType::DEF;
    if (id == "if")     return NeoTokType::IF;
    if (id == "else")   return NeoTokType::ELSE;
    if (id == "elif")   return NeoTokType::ELIF;
    if (id == "while")  return NeoTokType::WHILE;
    if (id == "for")    return NeoTokType::FOR;
    if (id == "in")     return NeoTokType::IN;
    if (id == "return") return NeoTokType::RETURN;
    if (id == "and")    return NeoTokType::AND;
    if (id == "or")     return NeoTokType::OR;
    if (id == "not")    return NeoTokType::NOT;
    if (id == "True"  || id == "true")  return NeoTokType::TRUE_KW;
    if (id == "False" || id == "false") return NeoTokType::FALSE_KW;
    if (id == "None"  || id == "none")  return NeoTokType::NONE_KW;
    if (id == "try")    return NeoTokType::TRY;
    if (id == "except") return NeoTokType::EXCEPT;
    if (id == "as")     return NeoTokType::AS;
    return NeoTokType::IDENTIFIER;
}

// ═════════════════════════════════════════════════════════════════
// Character-level helpers
// ═════════════════════════════════════════════════════════════════

char neo::NeoLexer::peek(int offset) const {
    size_t p = _pos + static_cast<size_t>(offset);
    if (p >= _src.size()) return '\0';
    return _src[p];
}

char neo::NeoLexer::advance() {
    char c = _src[_pos++];
    if (c == '\n') {
        // Caller handles line/col increment around newlines;
        // within a single token we just bump col.
        _col++;
    } else {
        _col++;
    }
    return c;
}

bool neo::NeoLexer::atEnd() const {
    return _pos >= _src.size();
}

// ═════════════════════════════════════════════════════════════════
// Token factory helpers (for code clarity)
// ═════════════════════════════════════════════════════════════════

NeoToken neo::NeoLexer::makeToken(NeoTokType t, const std::string& val) const {
    return NeoToken(t, val, _line, _col);
}

NeoToken neo::NeoLexer::makeToken(NeoTokType t, char c) const {
    return NeoToken(t, std::string(1, c), _line, _col);
}

NeoToken neo::NeoLexer::errorToken(const std::string& msg) const {
    return NeoToken(NeoTokType::ERROR, msg, _line, _col);
}
