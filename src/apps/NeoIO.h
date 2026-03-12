/**
 * NeoIO.h — File System & Data Serialization for NeoLanguage Phase 7.
 *
 * Provides NeoLanguage built-in functions for persistent file I/O:
 *
 *   File I/O:
 *     open(path, mode)   — Open a file. mode: "r" read, "w" write, "a" append.
 *                          Returns a file handle (Number), or -1 on error.
 *     read(handle)       — Read entire file as a String.
 *     write(handle, str) — Write a string to the file.
 *     close(handle)      — Close the file handle.
 *
 *   JSON serialization:
 *     json_encode(val)   — Serialize a NeoValue (Dict/List/Number/String/Bool/Null)
 *                          to a JSON string.
 *     json_decode(str)   — Parse a JSON string back into a NeoValue.
 *
 *   CSV data exchange:
 *     export_csv(matrix, path) — Write a List-of-Lists as a CSV file.
 *     import_csv(path)         — Read a CSV file; returns a List-of-Lists.
 *
 * File handle design:
 *   NeoIO keeps up to NEO_IO_MAX_FILES (8) open file slots.
 *   The handle returned by open() is a small integer index (0-7).
 *   On Arduino targets, all I/O uses LittleFS.
 *   On other targets, I/O uses standard C stdio.
 *
 * JSON format notes:
 *   · Dict  → {"key": value, ...}
 *   · List  → [a, b, c]
 *   · Number → 3.14
 *   · String → "text" (with \n, \t, \" escaping)
 *   · Bool  → true / false
 *   · Null  → null
 *   Nested structures are fully supported.
 *
 * Implementation:
 *   All platform-specific code (LittleFS) lives in NeoIO.cpp.
 *   This header is platform-agnostic.
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage Phase 7
 */
#pragma once

#include <cstdio>
#include <cstring>
#include <cctype>
#include <string>
#include <vector>
#include <sstream>
#include <map>
#include "NeoValue.h"

namespace NeoIO {

static constexpr int NEO_IO_MAX_FILES = 8;

// ════════════════════════════════════════════════════════════════════
// File I/O — implemented in NeoIO.cpp (platform-specific)
// ════════════════════════════════════════════════════════════════════

/**
 * open(path, mode) — open a file.
 * @return Handle index [0, NEO_IO_MAX_FILES) on success, -1 on failure.
 */
int  openFile (const char* path, const char* mode);

/**
 * read(handle) — read entire file into a std::string.
 * Returns empty string on error.
 */
std::string readFile(int handle);

/**
 * write(handle, str) — write a string to the file.
 * Returns number of bytes written, or -1 on error.
 */
int  writeFile(int handle, const char* text);

/**
 * close(handle) — close the file and free the slot.
 */
void closeFile(int handle);

// ════════════════════════════════════════════════════════════════════
// JSON serialisation — header-only, no platform dependencies
// ════════════════════════════════════════════════════════════════════

inline std::string jsonEncodeString(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    out += '"';
    return out;
}

// Forward declaration for recursion
inline std::string jsonEncode(const NeoValue& v);

inline std::string jsonEncode(const NeoValue& v) {
    if (v.isNull())   return "null";
    if (v.isBool())   return v.asBool() ? "true" : "false";
    if (v.isString()) return jsonEncodeString(v.asString());
    if (v.isNumeric() || v.isExact()) {
        char buf[64];
        double d = v.toDouble();
        if (d == std::floor(d) && std::fabs(d) < 1e15) {
            std::snprintf(buf, sizeof(buf), "%.0f", d);
        } else {
            std::snprintf(buf, sizeof(buf), "%.10g", d);
        }
        return buf;
    }
    if (v.isList() && v.asList()) {
        std::string out = "[";
        const auto& lst = *v.asList();
        for (size_t i = 0; i < lst.size(); ++i) {
            if (i) out += ",";
            out += jsonEncode(lst[i]);
        }
        out += "]";
        return out;
    }
    if (v.isDict() && v.asDict()) {
        std::string out = "{";
        bool first = true;
        for (const auto& kv : *v.asDict()) {
            if (!first) out += ",";
            first = false;
            out += jsonEncodeString(kv.first);
            out += ":";
            out += jsonEncode(kv.second);
        }
        out += "}";
        return out;
    }
    return jsonEncodeString(v.toString());
}

// ── JSON decoder (recursive descent parser) ──────────────────────

struct JsonParser {
    const char* src;
    int         pos;

    explicit JsonParser(const char* s) : src(s), pos(0) {}

    void skipWs() {
        while (src[pos] && (src[pos] == ' '  || src[pos] == '\t' ||
                            src[pos] == '\n' || src[pos] == '\r')) ++pos;
    }
    char peek() { skipWs(); return src[pos]; }
    char next() { skipWs(); return src[pos++]; }

    NeoValue parse() {
        char c = peek();
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') return parseString();
        if (c == 't') { pos += 4; return NeoValue::makeBool(true); }
        if (c == 'f') { pos += 5; return NeoValue::makeBool(false); }
        if (c == 'n') { pos += 4; return NeoValue::makeNull(); }
        return parseNumber();
    }

    NeoValue parseString() {
        next(); // consume '"'
        std::string s;
        while (src[pos] && src[pos] != '"') {
            if (src[pos] == '\\') {
                ++pos;
                switch (src[pos]) {
                    case '"':  s += '"';  break;
                    case '\\': s += '\\'; break;
                    case 'n':  s += '\n'; break;
                    case 'r':  s += '\r'; break;
                    case 't':  s += '\t'; break;
                    default:   s += src[pos]; break;
                }
            } else {
                s += src[pos];
            }
            ++pos;
        }
        if (src[pos] == '"') ++pos;
        return NeoValue::makeString(s);
    }

    NeoValue parseNumber() {
        skipWs();
        int start = pos;
        if (src[pos] == '-') ++pos;
        while (src[pos] && (src[pos] == '.' ||
               (src[pos] >= '0' && src[pos] <= '9') ||
               src[pos] == 'e' || src[pos] == 'E' ||
               src[pos] == '+' || src[pos] == '-')) ++pos;
        if (pos == start) return NeoValue::makeNull();
        std::string num(src + start, static_cast<size_t>(pos - start));
        return NeoValue::makeNumber(std::stod(num));
    }

    NeoValue parseArray() {
        next(); // consume '['
        auto* lst = new std::vector<NeoValue>();
        skipWs();
        if (peek() == ']') { next(); return NeoValue::makeList(lst); }
        while (true) {
            lst->push_back(parse());
            skipWs();
            if (peek() == ',') { next(); continue; }
            break;
        }
        if (peek() == ']') next();
        return NeoValue::makeList(lst);
    }

    NeoValue parseObject() {
        next(); // consume '{'
        auto* dict = new std::map<std::string, NeoValue>();
        skipWs();
        if (peek() == '}') { next(); return NeoValue::makeDict(dict); }
        while (true) {
            skipWs();
            std::string key;
            if (peek() == '"') {
                NeoValue kv = parseString();
                key = kv.isString() ? kv.asString() : kv.toString();
            }
            skipWs();
            if (peek() == ':') next();
            NeoValue val = parse();
            (*dict)[key] = val;
            skipWs();
            if (peek() == ',') { next(); continue; }
            break;
        }
        if (peek() == '}') next();
        return NeoValue::makeDict(dict);
    }
};

inline NeoValue jsonDecode(const std::string& s) {
    JsonParser p(s.c_str());
    return p.parse();
}

// ════════════════════════════════════════════════════════════════════
// CSV support — header-only
// ════════════════════════════════════════════════════════════════════

/**
 * Export a NeoValue List-of-Lists (matrix) to a CSV file at `path`.
 * @return true on success, false on failure.
 */
inline bool exportCsv(const NeoValue& matrix, const char* path) {
    if (!matrix.isList() || !matrix.asList()) return false;
    int h = openFile(path, "w");
    if (h < 0) return false;
    for (const NeoValue& row : *matrix.asList()) {
        std::string line;
        if (row.isList() && row.asList()) {
            bool first = true;
            for (const NeoValue& cell : *row.asList()) {
                if (!first) line += ",";
                first = false;
                if (cell.isString()) {
                    const std::string& s = cell.asString();
                    bool needsQuote = (s.find(',') != std::string::npos ||
                                       s.find('"') != std::string::npos ||
                                       s.find('\n') != std::string::npos);
                    if (needsQuote) {
                        line += '"';
                        for (char c : s) { if (c == '"') line += '"'; line += c; }
                        line += '"';
                    } else {
                        line += s;
                    }
                } else {
                    line += cell.toString();
                }
            }
        } else {
            line = row.toString();
        }
        line += "\n";
        writeFile(h, line.c_str());
    }
    closeFile(h);
    return true;
}

/**
 * Import a CSV file and return a List-of-Lists.
 */
inline NeoValue importCsv(const char* path) {
    int h = openFile(path, "r");
    if (h < 0) return NeoValue::makeNull();
    std::string raw = readFile(h);
    closeFile(h);

    auto* rows = new std::vector<NeoValue>();
    std::istringstream ss(raw);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        auto* cells = new std::vector<NeoValue>();
        std::istringstream ls(line);
        std::string cell;
        while (std::getline(ls, cell, ',')) {
            // Trim whitespace
            size_t start = cell.find_first_not_of(" \t\r\n");
            size_t end   = cell.find_last_not_of(" \t\r\n");
            if (start == std::string::npos) {
                cells->push_back(NeoValue::makeString(""));
            } else {
                std::string trimmed = cell.substr(start, end - start + 1);
                // Try numeric parse
                bool isNum = !trimmed.empty();
                for (char c : trimmed) {
                    if ((c < '0' || c > '9') && c != '.' &&
                        c != '-' && c != '+' && c != 'e' && c != 'E') {
                        isNum = false; break;
                    }
                }
                if (isNum) {
                    cells->push_back(NeoValue::makeNumber(std::stod(trimmed)));
                } else {
                    cells->push_back(NeoValue::makeString(trimmed));
                }
            }
        }
        rows->push_back(NeoValue::makeList(cells));
    }
    return NeoValue::makeList(rows);
}

} // namespace NeoIO
