/**
 * NeoUnits.h — Symbolic dimensional analysis engine for NeoLanguage.
 *
 * Implements a "Quantity" system: a numeric magnitude paired with a
 * dimensional signature (SI base units), enabling automatic unit
 * conversion and dimensional checking.
 *
 * Supported SI base units and their dimension index:
 *   0: m   (metre)
 *   1: kg  (kilogram)
 *   2: s   (second)
 *   3: A   (ampere)
 *   4: K   (kelvin)
 *   5: mol (mole)
 *   6: cd  (candela)
 *
 * Named units (SI + common derived + imperial):
 *   Length : m, cm, mm, km, in, ft, yd, mi
 *   Mass   : kg, g, mg, lb, oz
 *   Time   : s, ms, min, h, d
 *   Force  : N  (= kg·m/s²)
 *   Energy : J  (= kg·m²/s²),  cal, kcal, kWh
 *   Power  : W  (= kg·m²/s³)
 *   Pressure: Pa (= kg/(m·s²)),  bar, atm, psi
 *   Speed  : m/s, km/h, mph
 *   Temperature: K, C (Celsius), F (Fahrenheit) — conversion via offset
 *
 * Usage in NeoLanguage:
 *   a = unit(5, "m")          # 5 metres
 *   b = unit(200, "cm")       # 200 centimetres
 *   c = a + b                 # => Quantity(7, "m")  (auto-convert to first operand's unit)
 *   v = unit(10, "m") / unit(2, "s")  # => Quantity(5, "m/s")
 *   convert(a, "ft")          # => Quantity(16.404, "ft")
 *
 * Design notes:
 *   · NeoQuantity is heap-allocated with `new` and shared by pointer copy
 *     inside a NeoValue (like NeoValue::List).
 *   · The UnitMap stores integer powers (int8_t) for each SI dimension.
 *   · Temperature units with offsets (Celsius, Fahrenheit) are handled
 *     specially: only conversion, not arithmetic, is meaningful.
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage Phase 5 (Units)
 */
#pragma once

#include <cstdint>
#include <string>
#include <cmath>

// ════════════════════════════════════════════════════════════════════
// UnitMap — SI dimensional signature
// ════════════════════════════════════════════════════════════════════

/**
 * Represents a unit as a product of integer powers of SI base units:
 *   m^d[0] · kg^d[1] · s^d[2] · A^d[3] · K^d[4] · mol^d[5] · cd^d[6]
 */
struct UnitMap {
    int8_t d[7] = {0, 0, 0, 0, 0, 0, 0};

    bool isDimensionless() const {
        for (int i = 0; i < 7; ++i) if (d[i] != 0) return false;
        return true;
    }

    bool operator==(const UnitMap& o) const {
        for (int i = 0; i < 7; ++i) if (d[i] != o.d[i]) return false;
        return true;
    }

    bool operator!=(const UnitMap& o) const { return !(*this == o); }

    UnitMap operator*(const UnitMap& o) const {
        UnitMap r;
        for (int i = 0; i < 7; ++i) r.d[i] = d[i] + o.d[i];
        return r;
    }

    UnitMap operator/(const UnitMap& o) const {
        UnitMap r;
        for (int i = 0; i < 7; ++i) r.d[i] = d[i] - o.d[i];
        return r;
    }

    UnitMap pow(int exp) const {
        UnitMap r;
        for (int i = 0; i < 7; ++i) r.d[i] = static_cast<int8_t>(d[i] * exp);
        return r;
    }

    /// Build a human-readable unit string (e.g. "kg·m²/s²").
    std::string toString() const;
};

// ════════════════════════════════════════════════════════════════════
// NeoQuantity — magnitude + unit + symbol
// ════════════════════════════════════════════════════════════════════

/**
 * A physical quantity: magnitude in SI base units plus a UnitMap.
 * The `symbol` string carries the original user-friendly unit name
 * (e.g. "m/s", "N") for display purposes.
 */
struct NeoQuantity {
    double  magnitude = 0.0;   ///< Value expressed in SI base units
    UnitMap units;             ///< Dimensional signature
    std::string symbol;        ///< Display string (e.g. "N", "m/s")
};

// ════════════════════════════════════════════════════════════════════
// NeoUnits — unit registry & arithmetic
// ════════════════════════════════════════════════════════════════════

/**
 * Static utility class for unit parsing, conversion, and arithmetic.
 */
class NeoUnits {
public:
    // ── Parsing ───────────────────────────────────────────────────

    /**
     * Parse a unit string (e.g. "m", "kg", "m/s", "N", "J") into a
     * NeoQuantity that stores the magnitude of 1 of that unit expressed
     * in SI base units, plus the UnitMap.
     *
     * @param unitStr  Unit name string.
     * @param out      Output NeoQuantity (magnitude = SI conversion factor).
     * @return true if the unit string is recognised; false otherwise.
     */
    static bool parseUnit(const std::string& unitStr, NeoQuantity& out);

    /**
     * Create a NeoQuantity from a numeric value and a unit string.
     * magnitude is stored in SI base units.
     */
    static bool makeQuantity(double value, const std::string& unitStr,
                              NeoQuantity& out);

    // ── Conversion ────────────────────────────────────────────────

    /**
     * Convert a NeoQuantity to the given target unit.
     * Returns false if the dimensions are incompatible or the unit is unknown.
     *
     * @param qty       Source quantity (magnitude in SI).
     * @param targetStr Target unit string.
     * @param out       Result quantity (magnitude expressed in targetStr units).
     */
    static bool convert(const NeoQuantity& qty,
                        const std::string& targetStr,
                        NeoQuantity&       out);

    /**
     * Return the display value (magnitude in the original display unit).
     * If the symbol is the SI unit, returns qty.magnitude as-is.
     */
    static double displayValue(const NeoQuantity& qty);

    // ── Arithmetic ────────────────────────────────────────────────

    /**
     * Add two quantities.  The result is in the units of `a`.
     * Returns false if the units are dimensionally incompatible.
     */
    static bool add(const NeoQuantity& a, const NeoQuantity& b, NeoQuantity& out);

    /**
     * Subtract two quantities.  Result is in the units of `a`.
     */
    static bool sub(const NeoQuantity& a, const NeoQuantity& b, NeoQuantity& out);

    /**
     * Multiply two quantities (units combine).
     */
    static void mul(const NeoQuantity& a, const NeoQuantity& b, NeoQuantity& out);

    /**
     * Divide two quantities (units combine).
     * Returns false if b.magnitude ≈ 0.
     */
    static bool div(const NeoQuantity& a, const NeoQuantity& b, NeoQuantity& out);

    /**
     * Multiply a quantity by a dimensionless scalar.
     */
    static void scale(const NeoQuantity& a, double scalar, NeoQuantity& out);

    // ── Display ───────────────────────────────────────────────────

    /// Build a human-readable string for a NeoQuantity, e.g. "5 m" or "9.8 m/s²".
    static std::string toString(const NeoQuantity& qty);

    // ── Named unit lookup ─────────────────────────────────────────

    struct UnitInfo {
        const char* name;      ///< Unit name (e.g. "km")
        double      factor;    ///< SI conversion: 1 <unit> = factor <SI>
        UnitMap     dims;      ///< Dimensional signature
        bool        hasOffset; ///< true for Celsius / Fahrenheit
        double      offset;    ///< SI = factor * value + offset
    };

    /// Look up a base unit by name.  Returns nullptr if not found.
    static const UnitInfo* findUnit(const std::string& name);

private:
    NeoUnits() = delete;
    static const UnitInfo kUnits[];
    static const size_t   kUnitCount;
};
