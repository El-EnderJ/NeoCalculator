/**
 * CASInt.h — Hybrid BigInteger: int64_t fast-path + mbedtls_mpi overflow promotion.
 *
 * Hot path (95% of operations):
 *   All arithmetic uses native int64_t with GCC overflow builtins
 *   (__builtin_add_overflow, __builtin_mul_overflow, etc.) for
 *   ~2-cycle overflow detection on the carry flag.
 *
 * Cold path (overflow detected):
 *   Both operands promote to mbedtls_mpi (arbitrary precision),
 *   the operation is retried, and the result demotes back to int64_t
 *   if it fits (bitlen ≤ 63).
 *
 * Memory layout (ESP32-S3, 32-bit pointers):
 *   sizeof(CASInt) = 16 bytes  (int64_t + ptr + bool + padding)
 *   - Not promoted: 16 bytes total, zero heap allocation.
 *   - Promoted: 16 bytes + 12-byte mbedtls_mpi struct + limb array on heap.
 *
 * Current lifecycle:
 *   CASInt manages its own mbedtls_mpi via destructor. Future integration
 *   with SymExprArena's BigInt registry will take over lifecycle management
 *   for arena-allocated nodes.
 *
 * Requires: GCC or Clang (for __builtin_*_overflow intrinsics).
 *           On ESP32: mbedtls (bundled in ESP-IDF, no extra lib_deps).
 *           On native: overflow returns error state (no BigInt promotion).
 *
 * Part of: NumOS Elite OmniCAS — Phase 1 (BigInt Hybrid Engine)
 */

#pragma once

#include <cstdint>
#include <cmath>
#include <cstring>
#include <string>

#ifdef ARDUINO
  #include <mbedtls/bignum.h>
  #include <esp_heap_caps.h>
  #define CAS_HAS_BIGINT 1
#else
  #define CAS_HAS_BIGINT 0
#endif

namespace cas {

class CASInt {
public:
    // ════════════════════════════════════════════════════════════════
    // Construction / Destruction
    // ════════════════════════════════════════════════════════════════

    CASInt() : _val(0), _big(nullptr), _error(false) {}

    explicit CASInt(int64_t v) : _val(v), _big(nullptr), _error(false) {}

    static CASInt fromInt(int64_t v) { return CASInt(v); }

    ~CASInt() { releaseBig(); }

    // ── Copy ────────────────────────────────────────────────────────

    CASInt(const CASInt& o) : _val(o._val), _big(nullptr), _error(o._error) {
#if CAS_HAS_BIGINT
        if (o._big) {
            _big = allocBig();
            if (_big) {
                if (mbedtls_mpi_copy(_big, o._big) != 0) {
                    freeMpi(_big); _big = nullptr; _error = true;
                }
            } else {
                _error = true;
            }
        }
#endif
    }

    CASInt& operator=(const CASInt& o) {
        if (this != &o) {
            releaseBig();
            _val = o._val;
            _error = o._error;
#if CAS_HAS_BIGINT
            if (o._big) {
                _big = allocBig();
                if (_big) {
                    if (mbedtls_mpi_copy(_big, o._big) != 0) {
                        freeMpi(_big); _big = nullptr; _error = true;
                    }
                } else {
                    _error = true;
                }
            }
#endif
        }
        return *this;
    }

    // ── Move ────────────────────────────────────────────────────────

    CASInt(CASInt&& o) noexcept
        : _val(o._val), _big(o._big), _error(o._error)
    {
        o._big = nullptr;
        o._val = 0;
    }

    CASInt& operator=(CASInt&& o) noexcept {
        if (this != &o) {
            releaseBig();
            _val   = o._val;
            _big   = o._big;
            _error = o._error;
            o._big = nullptr;
            o._val = 0;
        }
        return *this;
    }

    // ════════════════════════════════════════════════════════════════
    // Queries
    // ════════════════════════════════════════════════════════════════

    bool isPromoted() const { return _big != nullptr; }
    bool hasError()   const { return _error; }

    bool isZero() const {
#if CAS_HAS_BIGINT
        if (_big) return mbedtls_mpi_cmp_int(_big, 0) == 0;
#endif
        return _val == 0;
    }

    bool isOne() const {
#if CAS_HAS_BIGINT
        if (_big) return mbedtls_mpi_cmp_int(_big, 1) == 0;
#endif
        return _val == 1;
    }

    bool isNegative() const {
#if CAS_HAS_BIGINT
        if (_big) return _big->s < 0 && !isZero();
#endif
        return _val < 0;
    }

    // ════════════════════════════════════════════════════════════════
    // Conversion
    // ════════════════════════════════════════════════════════════════

    /// Extract int64 value. Only meaningful when !isPromoted().
    int64_t toInt64() const {
#if CAS_HAS_BIGINT
        if (_big) return bigToInt64(_big);
#endif
        return _val;
    }

    /// Approximate as double (always valid, may lose precision for large BigInts).
    double toDouble() const {
#if CAS_HAS_BIGINT
        if (_big) return bigToDouble(_big);
#endif
        return static_cast<double>(_val);
    }

    /// Number of bits needed to represent the absolute value.
    size_t bitlen() const {
#if CAS_HAS_BIGINT
        if (_big) return mbedtls_mpi_bitlen(_big);
#endif
        if (_val == 0) return 0;
        // Compute bitlen for |_val|
        uint64_t v = (_val > 0)
            ? static_cast<uint64_t>(_val)
            : ((_val == INT64_MIN)
                ? static_cast<uint64_t>(INT64_MAX) + 1
                : static_cast<uint64_t>(-_val));
        size_t n = 0;
        while (v) { ++n; v >>= 1; }
        return n;
    }

    // ════════════════════════════════════════════════════════════════
    // Arithmetic — static functions, return new CASInt by value.
    // All propagate error state and auto-promote on overflow.
    // ════════════════════════════════════════════════════════════════

    // ── Addition ────────────────────────────────────────────────────

    static CASInt add(const CASInt& a, const CASInt& b) {
        if (a._error || b._error) return makeError();
#if CAS_HAS_BIGINT
        if (a._big || b._big) return addBig(a, b);
#endif
        int64_t r;
        if (__builtin_add_overflow(a._val, b._val, &r)) {
#if CAS_HAS_BIGINT
            return addBig(a, b);
#else
            return makeError();
#endif
        }
        return CASInt(r);
    }

    // ── Subtraction ─────────────────────────────────────────────────

    static CASInt sub(const CASInt& a, const CASInt& b) {
        if (a._error || b._error) return makeError();
#if CAS_HAS_BIGINT
        if (a._big || b._big) return subBig(a, b);
#endif
        int64_t r;
        if (__builtin_sub_overflow(a._val, b._val, &r)) {
#if CAS_HAS_BIGINT
            return subBig(a, b);
#else
            return makeError();
#endif
        }
        return CASInt(r);
    }

    // ── Multiplication ──────────────────────────────────────────────

    static CASInt mul(const CASInt& a, const CASInt& b) {
        if (a._error || b._error) return makeError();
#if CAS_HAS_BIGINT
        if (a._big || b._big) return mulBig(a, b);
#endif
        int64_t r;
        if (__builtin_mul_overflow(a._val, b._val, &r)) {
#if CAS_HAS_BIGINT
            return mulBig(a, b);
#else
            return makeError();
#endif
        }
        return CASInt(r);
    }

    // ── Integer Division (truncation toward zero) ───────────────────

    static CASInt div(const CASInt& a, const CASInt& b) {
        if (a._error || b._error) return makeError();
        if (b.isZero()) return makeError();  // division by zero
#if CAS_HAS_BIGINT
        if (a._big || b._big) return divBig(a, b);
#endif
        // INT64_MIN / -1 overflows (result is INT64_MAX + 1)
        if (a._val == INT64_MIN && b._val == -1) {
#if CAS_HAS_BIGINT
            return divBig(a, b);
#else
            return makeError();
#endif
        }
        return CASInt(a._val / b._val);
    }

    // ── Modulo (sign of dividend, matches C99) ──────────────────────

    static CASInt mod(const CASInt& a, const CASInt& b) {
        if (a._error || b._error) return makeError();
        if (b.isZero()) return makeError();
#if CAS_HAS_BIGINT
        if (a._big || b._big) return modBig(a, b);
#endif
        if (a._val == INT64_MIN && b._val == -1) return CASInt(0);
        return CASInt(a._val % b._val);
    }

    // ── Greatest Common Divisor (always ≥ 0) ────────────────────────

    static CASInt gcd(const CASInt& a, const CASInt& b) {
        if (a._error || b._error) return makeError();
        if (a.isZero()) return absVal(b);
        if (b.isZero()) return absVal(a);
#if CAS_HAS_BIGINT
        if (a._big || b._big) return gcdBig(a, b);
#endif
        // |INT64_MIN| can't be represented as int64_t → promote
        if (a._val == INT64_MIN || b._val == INT64_MIN) {
#if CAS_HAS_BIGINT
            return gcdBig(a, b);
#else
            return makeError();
#endif
        }
        // Euclidean algorithm on positive int64
        int64_t x = a._val < 0 ? -a._val : a._val;
        int64_t y = b._val < 0 ? -b._val : b._val;
        while (y != 0) {
            int64_t t = y;
            y = x % y;
            x = t;
        }
        return CASInt(x);
    }

    // ── Absolute Value ──────────────────────────────────────────────

    static CASInt absVal(const CASInt& a) {
        if (a._error) return makeError();
#if CAS_HAS_BIGINT
        if (a._big) {
            CASInt r(a);
            if (r._big) r._big->s = 1;
            return r;
        }
#endif
        if (a._val == INT64_MIN) {
#if CAS_HAS_BIGINT
            return promoteAndNegate(a, true);
#else
            return makeError();
#endif
        }
        return CASInt(a._val < 0 ? -a._val : a._val);
    }

    // ── Negation ────────────────────────────────────────────────────

    static CASInt neg(const CASInt& a) {
        if (a._error) return makeError();
#if CAS_HAS_BIGINT
        if (a._big) {
            CASInt r(a);
            if (r._big && !r.isZero()) r._big->s = -(r._big->s);
            return r;
        }
#endif
        int64_t r;
        if (__builtin_sub_overflow(static_cast<int64_t>(0), a._val, &r)) {
            // Only INT64_MIN triggers this
#if CAS_HAS_BIGINT
            return promoteAndNegate(a, false);
#else
            return makeError();
#endif
        }
        return CASInt(r);
    }

    // ════════════════════════════════════════════════════════════════
    // Comparison — returns -1, 0, or +1
    // ════════════════════════════════════════════════════════════════

    static int cmp(const CASInt& a, const CASInt& b) {
        if (a._error || b._error) return 0;
#if CAS_HAS_BIGINT
        if (a._big && b._big)
            return mbedtls_mpi_cmp_mpi(a._big, b._big);
        if (a._big)
            return cmpBigInt64(a._big, b._val);
        if (b._big)
            return -cmpBigInt64(b._big, a._val);
#endif
        if (a._val < b._val) return -1;
        if (a._val > b._val) return  1;
        return 0;
    }

    bool operator==(const CASInt& o) const { return cmp(*this, o) == 0; }
    bool operator!=(const CASInt& o) const { return cmp(*this, o) != 0; }
    bool operator< (const CASInt& o) const { return cmp(*this, o) <  0; }
    bool operator<=(const CASInt& o) const { return cmp(*this, o) <= 0; }
    bool operator> (const CASInt& o) const { return cmp(*this, o) >  0; }
    bool operator>=(const CASInt& o) const { return cmp(*this, o) >= 0; }

    // ════════════════════════════════════════════════════════════════
    // Debug
    // ════════════════════════════════════════════════════════════════

    std::string toString() const {
        if (_error) return "<error>";
#if CAS_HAS_BIGINT
        if (_big) {
            char buf[128];
            size_t olen = 0;
            int ret = mbedtls_mpi_write_string(_big, 10, buf, sizeof(buf), &olen);
            if (ret == 0) return std::string(buf);
            // Buffer too small — allocate dynamically
            if (ret == MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL && olen > 0) {
                std::string s(olen, '\0');
                ret = mbedtls_mpi_write_string(_big, 10, &s[0], olen, &olen);
                if (ret == 0) { s.resize(std::strlen(s.c_str())); return s; }
            }
            return "<big>";
        }
#endif
        return std::to_string(_val);
    }

private:
    int64_t _val;                   // Hot-path value (used when _big == nullptr)
#if CAS_HAS_BIGINT
    mbedtls_mpi* _big;             // Promoted BigInt (nullptr = not promoted)
#else
    void* _big;                    // Always nullptr on native builds
#endif
    bool _error;                   // True if an operation failed irrecoverably

    // ── Error factory ───────────────────────────────────────────────

    static CASInt makeError() {
        CASInt r;
        r._error = true;
        return r;
    }

    // ── Memory management ───────────────────────────────────────────

    void releaseBig() {
#if CAS_HAS_BIGINT
        if (_big) {
            mbedtls_mpi_free(_big);   // Free internal limb array
            free(_big);               // Free the struct itself
            _big = nullptr;
        }
#endif
    }

#if CAS_HAS_BIGINT

    // ────────────────────────────────────────────────────────────────
    // BigInt helpers (cold path) — only compiled on ESP32
    // ────────────────────────────────────────────────────────────────

    /// Allocate and initialize an mbedtls_mpi struct in PSRAM (or heap fallback).
    static mbedtls_mpi* allocBig() {
        mbedtls_mpi* m = static_cast<mbedtls_mpi*>(
            heap_caps_calloc(1, sizeof(mbedtls_mpi),
                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!m) {
            m = static_cast<mbedtls_mpi*>(calloc(1, sizeof(mbedtls_mpi)));
        }
        if (m) mbedtls_mpi_init(m);
        return m;
    }

    /// Free an mbedtls_mpi struct (limbs + struct).
    static void freeMpi(mbedtls_mpi* m) {
        if (m) {
            mbedtls_mpi_free(m);
            free(m);
        }
    }

    /// Set an mbedtls_mpi from an int64_t value.
    /// `m` must already be initialized (mbedtls_mpi_init called).
    /// Returns 0 on success, negative mbedtls error on failure.
    static int setInt64(mbedtls_mpi* m, int64_t v) {
        // mbedtls_mpi_lset takes mbedtls_mpi_sint = int32_t on ESP32.
        // For values that fit, use the fast path.
        if (v >= INT32_MIN && v <= INT32_MAX) {
            return mbedtls_mpi_lset(m, static_cast<int32_t>(v));
        }
        // Decompose int64 into 8 big-endian bytes for mbedtls_mpi_read_binary.
        uint64_t abs_v;
        int sign;
        if (v < 0) {
            abs_v = (v == INT64_MIN)
                ? (static_cast<uint64_t>(INT64_MAX) + 1)
                : static_cast<uint64_t>(-v);
            sign = -1;
        } else {
            abs_v = static_cast<uint64_t>(v);
            sign = 1;
        }
        uint8_t buf[8];
        for (int i = 7; i >= 0; --i) {
            buf[i] = static_cast<uint8_t>(abs_v & 0xFF);
            abs_v >>= 8;
        }
        int ret = mbedtls_mpi_read_binary(m, buf, 8);
        if (ret != 0) return ret;
        m->s = sign;
        return 0;
    }

    /// Load a CASInt's value into a pre-initialized mbedtls_mpi.
    /// Returns 0 on success.
    static int loadMpi(mbedtls_mpi* out, const CASInt& v) {
        if (v._big) {
            return mbedtls_mpi_copy(out, v._big);
        }
        return setInt64(out, v._val);
    }

    /// Extract int64 from an mbedtls_mpi (only valid when fitsInt64).
    static int64_t bigToInt64(const mbedtls_mpi* m) {
        size_t n = mbedtls_mpi_size(m);  // byte count
        if (n == 0) return 0;
        if (n > 8) return 0;  // Shouldn't call this if doesn't fit

        uint8_t buf[8] = {0};
        mbedtls_mpi_write_binary(m, buf + (8 - n), n);

        uint64_t abs_v = 0;
        for (int i = 0; i < 8; ++i) {
            abs_v = (abs_v << 8) | buf[i];
        }
        if (m->s < 0) {
            return -static_cast<int64_t>(abs_v);
        }
        return static_cast<int64_t>(abs_v);
    }

    /// Convert an mbedtls_mpi to double (approximate for large values).
    static double bigToDouble(const mbedtls_mpi* m) {
        size_t bits = mbedtls_mpi_bitlen(m);
        if (bits == 0) return 0.0;

        // For values ≤ 53 bits: exact double conversion via int64 path
        if (bits <= 53) {
            size_t n = mbedtls_mpi_size(m);
            uint8_t buf[8] = {0};
            mbedtls_mpi_write_binary(m, buf + (8 - n), n);
            uint64_t abs_v = 0;
            for (int i = 0; i < 8; ++i)
                abs_v = (abs_v << 8) | buf[i];
            double d = static_cast<double>(abs_v);
            return (m->s < 0) ? -d : d;
        }

        // For larger values: read top 53 significant bits + scale
        double d = 0.0;
        int start = static_cast<int>(bits) - 1;
        int end   = start - 52;  // 53 bits total
        if (end < 0) end = 0;
        for (int i = start; i >= end; --i) {
            d = d * 2.0 + static_cast<double>(mbedtls_mpi_get_bit(m, i));
        }
        if (end > 0) {
            d = ldexp(d, end);
        }
        return (m->s < 0) ? -d : d;
    }

    /// Check if an mbedtls_mpi value fits in int64_t.
    /// Accepts |value| < 2^63 (conservative: excludes INT64_MIN).
    static bool fitsInt64(const mbedtls_mpi* m) {
        return mbedtls_mpi_bitlen(m) <= 63;
    }

    /// Wrap an mbedtls_mpi result: demote to int64 if it fits, else keep promoted.
    /// Takes ownership of `m`. Caller must NOT free `m` after this call.
    static CASInt wrapBig(mbedtls_mpi* m) {
        if (fitsInt64(m)) {
            int64_t v = bigToInt64(m);
            freeMpi(m);
            return CASInt(v);
        }
        CASInt r;
        r._big = m;
        return r;
    }

    /// Compare promoted mbedtls_mpi with an int64_t value.
    static int cmpBigInt64(const mbedtls_mpi* big, int64_t val) {
        // Fast path: value fits in int32 → use mbedtls_mpi_cmp_int directly
        if (val >= INT32_MIN && val <= INT32_MAX) {
            return mbedtls_mpi_cmp_int(big, static_cast<int32_t>(val));
        }
        // General case: convert to temp mpi and compare
        mbedtls_mpi tmp;
        mbedtls_mpi_init(&tmp);
        setInt64(&tmp, val);
        int r = mbedtls_mpi_cmp_mpi(big, &tmp);
        mbedtls_mpi_free(&tmp);
        return r;
    }

    /// Promote an int64 CASInt and negate/abs. Used by neg() and absVal()
    /// when the int64 value is INT64_MIN (whose negation overflows).
    /// If `makePositive` is true → abs, else → negate.
    static CASInt promoteAndNegate(const CASInt& a, bool makePositive) {
        CASInt r;
        r._big = allocBig();
        if (!r._big) return makeError();
        if (setInt64(r._big, a._val) != 0) {
            freeMpi(r._big); r._big = nullptr;
            return makeError();
        }
        if (makePositive) {
            r._big->s = 1;
        } else {
            // Flip sign (INT64_MIN is negative → result is positive)
            r._big->s = -(r._big->s);
        }
        return r;
    }

    // ────────────────────────────────────────────────────────────────
    // BigInt arithmetic fallbacks
    //
    // Pattern: convert both operands to stack-local mbedtls_mpi temps,
    // allocate a heap mbedtls_mpi for the result, perform the operation,
    // free the temps, and demote the result if it fits in int64.
    // ────────────────────────────────────────────────────────────────

    static CASInt addBig(const CASInt& a, const CASInt& b) {
        mbedtls_mpi ma, mb;
        mbedtls_mpi_init(&ma);
        mbedtls_mpi_init(&mb);

        if (loadMpi(&ma, a) != 0 || loadMpi(&mb, b) != 0) {
            mbedtls_mpi_free(&ma); mbedtls_mpi_free(&mb);
            return makeError();
        }

        mbedtls_mpi* r = allocBig();
        if (!r) {
            mbedtls_mpi_free(&ma); mbedtls_mpi_free(&mb);
            return makeError();
        }

        int ret = mbedtls_mpi_add_mpi(r, &ma, &mb);
        mbedtls_mpi_free(&ma);
        mbedtls_mpi_free(&mb);

        if (ret != 0) { freeMpi(r); return makeError(); }
        return wrapBig(r);
    }

    static CASInt subBig(const CASInt& a, const CASInt& b) {
        mbedtls_mpi ma, mb;
        mbedtls_mpi_init(&ma);
        mbedtls_mpi_init(&mb);

        if (loadMpi(&ma, a) != 0 || loadMpi(&mb, b) != 0) {
            mbedtls_mpi_free(&ma); mbedtls_mpi_free(&mb);
            return makeError();
        }

        mbedtls_mpi* r = allocBig();
        if (!r) {
            mbedtls_mpi_free(&ma); mbedtls_mpi_free(&mb);
            return makeError();
        }

        int ret = mbedtls_mpi_sub_mpi(r, &ma, &mb);
        mbedtls_mpi_free(&ma);
        mbedtls_mpi_free(&mb);

        if (ret != 0) { freeMpi(r); return makeError(); }
        return wrapBig(r);
    }

    static CASInt mulBig(const CASInt& a, const CASInt& b) {
        mbedtls_mpi ma, mb;
        mbedtls_mpi_init(&ma);
        mbedtls_mpi_init(&mb);

        if (loadMpi(&ma, a) != 0 || loadMpi(&mb, b) != 0) {
            mbedtls_mpi_free(&ma); mbedtls_mpi_free(&mb);
            return makeError();
        }

        mbedtls_mpi* r = allocBig();
        if (!r) {
            mbedtls_mpi_free(&ma); mbedtls_mpi_free(&mb);
            return makeError();
        }

        int ret = mbedtls_mpi_mul_mpi(r, &ma, &mb);
        mbedtls_mpi_free(&ma);
        mbedtls_mpi_free(&mb);

        if (ret != 0) { freeMpi(r); return makeError(); }
        return wrapBig(r);
    }

    static CASInt divBig(const CASInt& a, const CASInt& b) {
        mbedtls_mpi ma, mb;
        mbedtls_mpi_init(&ma);
        mbedtls_mpi_init(&mb);

        if (loadMpi(&ma, a) != 0 || loadMpi(&mb, b) != 0) {
            mbedtls_mpi_free(&ma); mbedtls_mpi_free(&mb);
            return makeError();
        }

        mbedtls_mpi* q = allocBig();
        if (!q) {
            mbedtls_mpi_free(&ma); mbedtls_mpi_free(&mb);
            return makeError();
        }

        // mbedtls_mpi_div_mpi(Q, R, A, B) → A = Q*B + R
        // Pass NULL for R since we only want the quotient.
        int ret = mbedtls_mpi_div_mpi(q, nullptr, &ma, &mb);
        mbedtls_mpi_free(&ma);
        mbedtls_mpi_free(&mb);

        if (ret != 0) { freeMpi(q); return makeError(); }
        return wrapBig(q);
    }

    static CASInt modBig(const CASInt& a, const CASInt& b) {
        mbedtls_mpi ma, mb;
        mbedtls_mpi_init(&ma);
        mbedtls_mpi_init(&mb);

        if (loadMpi(&ma, a) != 0 || loadMpi(&mb, b) != 0) {
            mbedtls_mpi_free(&ma); mbedtls_mpi_free(&mb);
            return makeError();
        }

        mbedtls_mpi* r = allocBig();
        if (!r) {
            mbedtls_mpi_free(&ma); mbedtls_mpi_free(&mb);
            return makeError();
        }

        // Pass NULL for Q since we only want the remainder.
        int ret = mbedtls_mpi_div_mpi(nullptr, r, &ma, &mb);
        mbedtls_mpi_free(&ma);
        mbedtls_mpi_free(&mb);

        if (ret != 0) { freeMpi(r); return makeError(); }
        return wrapBig(r);
    }

    static CASInt gcdBig(const CASInt& a, const CASInt& b) {
        mbedtls_mpi ma, mb;
        mbedtls_mpi_init(&ma);
        mbedtls_mpi_init(&mb);

        if (loadMpi(&ma, a) != 0 || loadMpi(&mb, b) != 0) {
            mbedtls_mpi_free(&ma); mbedtls_mpi_free(&mb);
            return makeError();
        }

        // mbedtls_mpi_gcd requires non-negative inputs
        ma.s = 1;
        mb.s = 1;

        mbedtls_mpi* g = allocBig();
        if (!g) {
            mbedtls_mpi_free(&ma); mbedtls_mpi_free(&mb);
            return makeError();
        }

        int ret = mbedtls_mpi_gcd(g, &ma, &mb);
        mbedtls_mpi_free(&ma);
        mbedtls_mpi_free(&mb);

        if (ret != 0) { freeMpi(g); return makeError(); }
        return wrapBig(g);
    }

#endif  // CAS_HAS_BIGINT
};

} // namespace cas
