/**
 * NeoBitwise.h — Bitwise helper functions for NeoLanguage Phase 6.
 *
 * Provides bit-manipulation functions useful for embedded systems
 * and firmware / hardware engineering:
 *
 *   bit_get(val, n)   — returns 1 if bit n of val is set, else 0
 *   bit_set(val, n)   — returns val with bit n set (to 1)
 *   bit_clear(val, n) — returns val with bit n cleared (to 0)
 *   bit_toggle(val, n)— returns val with bit n toggled
 *   bit_count(val)    — returns number of set bits (popcount)
 *   to_bin(val)       — returns binary string "0b1010..."
 *   to_hex(val)       — returns hex string "0xFF..."
 *
 * All functions operate on the integer-truncated value of their
 * numeric argument.  Floating-point inputs are silently truncated.
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage Phase 6 (Electronics Toolkit)
 */
#pragma once

#include <cstdint>
#include <string>
#include "NeoValue.h"

namespace NeoBitwise {

// ════════════════════════════════════════════════════════════════════
// Helpers
// ════════════════════════════════════════════════════════════════════

inline int64_t toInt(const NeoValue& v) {
    if (v.isNumber()) return static_cast<int64_t>(v.asNum());
    if (v.isExact())  return v.asExact().toInt64();
    if (v.isBool())   return v.asBool() ? 1 : 0;
    return static_cast<int64_t>(v.toDouble());
}

// ════════════════════════════════════════════════════════════════════
// Bit manipulation
// ════════════════════════════════════════════════════════════════════

/// Returns 1 if bit n of val is set, else 0.
inline NeoValue bitGet(const NeoValue& val, const NeoValue& n) {
    int64_t v = toInt(val);
    int64_t b = toInt(n);
    if (b < 0 || b >= 64) return NeoValue::makeNumber(0.0);
    return NeoValue::makeNumber(static_cast<double>((v >> b) & 1));
}

/// Returns val with bit n set to 1.
inline NeoValue bitSet(const NeoValue& val, const NeoValue& n) {
    int64_t v = toInt(val);
    int64_t b = toInt(n);
    if (b >= 0 && b < 64) v |= (int64_t(1) << b);
    return NeoValue::makeNumber(static_cast<double>(v));
}

/// Returns val with bit n cleared to 0.
inline NeoValue bitClear(const NeoValue& val, const NeoValue& n) {
    int64_t v = toInt(val);
    int64_t b = toInt(n);
    if (b >= 0 && b < 64) v &= ~(int64_t(1) << b);
    return NeoValue::makeNumber(static_cast<double>(v));
}

/// Returns val with bit n toggled.
inline NeoValue bitToggle(const NeoValue& val, const NeoValue& n) {
    int64_t v = toInt(val);
    int64_t b = toInt(n);
    if (b >= 0 && b < 64) v ^= (int64_t(1) << b);
    return NeoValue::makeNumber(static_cast<double>(v));
}

/// Returns number of set bits (population count).
inline NeoValue bitCount(const NeoValue& val) {
    uint64_t v = static_cast<uint64_t>(toInt(val));
    int count = 0;
    while (v) { count += (v & 1); v >>= 1; }
    return NeoValue::makeNumber(static_cast<double>(count));
}

/// Converts val to binary string "0b1010".
inline NeoValue toBin(const NeoValue& val) {
    int64_t v = toInt(val);
    if (v == 0) return NeoValue::makeString("0b0");
    bool negative = (v < 0);
    uint64_t uv = static_cast<uint64_t>(negative ? -v : v);
    std::string bits;
    while (uv > 0) { bits = (char)('0' + (uv & 1)) + bits; uv >>= 1; }
    std::string result = negative ? "-0b" : "0b";
    result += bits;
    return NeoValue::makeString(result);
}

/// Converts val to hex string "0xFF".
inline NeoValue toHex(const NeoValue& val) {
    int64_t v = toInt(val);
    char buf[32];
    if (v < 0) {
        std::snprintf(buf, sizeof(buf), "-0x%llX", static_cast<unsigned long long>(-v));
    } else {
        std::snprintf(buf, sizeof(buf), "0x%llX", static_cast<unsigned long long>(v));
    }
    return NeoValue::makeString(std::string(buf));
}

} // namespace NeoBitwise
