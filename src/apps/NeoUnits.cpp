/**
 * NeoUnits.cpp — Symbolic dimensional analysis engine for NeoLanguage.
 *
 * See NeoUnits.h for the public API.
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage Phase 5 (Units)
 */

#include "NeoUnits.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <cctype>

// ════════════════════════════════════════════════════════════════════
// UnitMap::toString
// ════════════════════════════════════════════════════════════════════

static const char* kBaseName[7] = { "m", "kg", "s", "A", "K", "mol", "cd" };

std::string UnitMap::toString() const {
    std::string num, den;
    for (int i = 0; i < 7; ++i) {
        if (d[i] == 0) continue;
        char buf[16];
        if (d[i] > 0) {
            if (d[i] == 1) {
                num += kBaseName[i];
            } else {
                std::snprintf(buf, sizeof(buf), "%s^%d", kBaseName[i], (int)d[i]);
                num += buf;
            }
            num += '\xB7';  // middle dot (UTF-8: ·)
        } else {
            if (d[i] == -1) {
                den += kBaseName[i];
            } else {
                std::snprintf(buf, sizeof(buf), "%s^%d", kBaseName[i], (int)(-d[i]));
                den += buf;
            }
            den += '\xB7';
        }
    }
    // Remove trailing dots
    if (!num.empty() && num.back() == '\xB7') num.pop_back();
    if (!den.empty() && den.back() == '\xB7') den.pop_back();

    if (num.empty() && den.empty()) return "1";
    if (den.empty()) return num;
    if (num.empty()) return "1/" + den;
    return num + "/" + den;
}

// ════════════════════════════════════════════════════════════════════
// Unit registry
// ════════════════════════════════════════════════════════════════════

//                     dim:  m   kg   s   A   K  mol  cd
static UnitMap dimLength()    { UnitMap u; u.d[0]=1; return u; }
static UnitMap dimMass()      { UnitMap u; u.d[1]=1; return u; }
static UnitMap dimTime()      { UnitMap u; u.d[2]=1; return u; }
static UnitMap dimCurrent()   { UnitMap u; u.d[3]=1; return u; }
static UnitMap dimTemp()      { UnitMap u; u.d[4]=1; return u; }
static UnitMap dimMole()      { UnitMap u; u.d[5]=1; return u; }
static UnitMap dimCandela()   { UnitMap u; u.d[6]=1; return u; }
static UnitMap dimForce()     { UnitMap u; u.d[0]=1; u.d[1]=1; u.d[2]=-2; return u; }
static UnitMap dimEnergy()    { UnitMap u; u.d[0]=2; u.d[1]=1; u.d[2]=-2; return u; }
static UnitMap dimPower()     { UnitMap u; u.d[0]=2; u.d[1]=1; u.d[2]=-3; return u; }
static UnitMap dimPressure()  { UnitMap u; u.d[0]=-1; u.d[1]=1; u.d[2]=-2; return u; }
static UnitMap dimVelocity()  { UnitMap u; u.d[0]=1; u.d[2]=-1; return u; }
static UnitMap dimAccel()     { UnitMap u; u.d[0]=1; u.d[2]=-2; return u; }
static UnitMap dimArea()      { UnitMap u; u.d[0]=2; return u; }
static UnitMap dimVolume()    { UnitMap u; u.d[0]=3; return u; }
static UnitMap dimCharge()    { UnitMap u; u.d[2]=1; u.d[3]=1; return u; }
static UnitMap dimVoltage()   { UnitMap u; u.d[0]=2; u.d[1]=1; u.d[2]=-3; u.d[3]=-1; return u; }
static UnitMap dimFrequency() { UnitMap u; u.d[2]=-1; return u; }
static UnitMap dimNone()      { return UnitMap{}; }

const NeoUnits::UnitInfo NeoUnits::kUnits[] = {
    // Length
    { "m",   1.0,           dimLength(),   false, 0.0 },
    { "cm",  0.01,          dimLength(),   false, 0.0 },
    { "mm",  0.001,         dimLength(),   false, 0.0 },
    { "km",  1000.0,        dimLength(),   false, 0.0 },
    { "um",  1e-6,          dimLength(),   false, 0.0 },
    { "nm",  1e-9,          dimLength(),   false, 0.0 },
    { "in",  0.0254,        dimLength(),   false, 0.0 },
    { "ft",  0.3048,        dimLength(),   false, 0.0 },
    { "yd",  0.9144,        dimLength(),   false, 0.0 },
    { "mi",  1609.344,      dimLength(),   false, 0.0 },
    // Mass
    { "kg",  1.0,           dimMass(),     false, 0.0 },
    { "g",   0.001,         dimMass(),     false, 0.0 },
    { "mg",  1e-6,          dimMass(),     false, 0.0 },
    { "t",   1000.0,        dimMass(),     false, 0.0 },  // metric tonne
    { "lb",  0.45359237,    dimMass(),     false, 0.0 },
    { "oz",  0.028349523,   dimMass(),     false, 0.0 },
    // Time
    { "s",   1.0,           dimTime(),     false, 0.0 },
    { "ms",  0.001,         dimTime(),     false, 0.0 },
    { "us",  1e-6,          dimTime(),     false, 0.0 },
    { "min", 60.0,          dimTime(),     false, 0.0 },
    { "h",   3600.0,        dimTime(),     false, 0.0 },
    { "d",   86400.0,       dimTime(),     false, 0.0 },
    // Temperature
    { "K",   1.0,           dimTemp(),     false, 0.0 },
    // Celsius: SI = value + 273.15
    { "C",   1.0,           dimTemp(),     true,  273.15 },
    // Fahrenheit: SI = (value + 459.67) * 5/9
    { "F",   5.0/9.0,       dimTemp(),     true,  459.67 * 5.0/9.0 },
    // Current
    { "A",   1.0,           dimCurrent(),  false, 0.0 },
    { "mA",  0.001,         dimCurrent(),  false, 0.0 },
    // Amount of substance
    { "mol", 1.0,           dimMole(),     false, 0.0 },
    { "mmol",0.001,         dimMole(),     false, 0.0 },
    // Luminous intensity
    { "cd",  1.0,           dimCandela(),  false, 0.0 },
    // Force
    { "N",   1.0,           dimForce(),    false, 0.0 },
    { "kN",  1000.0,        dimForce(),    false, 0.0 },
    { "lbf", 4.44822,       dimForce(),    false, 0.0 },
    // Energy
    { "J",   1.0,           dimEnergy(),   false, 0.0 },
    { "kJ",  1000.0,        dimEnergy(),   false, 0.0 },
    { "MJ",  1e6,           dimEnergy(),   false, 0.0 },
    { "cal", 4.184,         dimEnergy(),   false, 0.0 },
    { "kcal",4184.0,        dimEnergy(),   false, 0.0 },
    { "kWh", 3.6e6,         dimEnergy(),   false, 0.0 },
    { "eV",  1.602176634e-19,dimEnergy(),  false, 0.0 },
    // Power
    { "W",   1.0,           dimPower(),    false, 0.0 },
    { "kW",  1000.0,        dimPower(),    false, 0.0 },
    { "MW",  1e6,           dimPower(),    false, 0.0 },
    { "hp",  745.69987,     dimPower(),    false, 0.0 },
    // Pressure
    { "Pa",  1.0,           dimPressure(), false, 0.0 },
    { "kPa", 1000.0,        dimPressure(), false, 0.0 },
    { "MPa", 1e6,           dimPressure(), false, 0.0 },
    { "bar", 1e5,           dimPressure(), false, 0.0 },
    { "atm", 101325.0,      dimPressure(), false, 0.0 },
    { "psi", 6894.757,      dimPressure(), false, 0.0 },
    { "mmHg",133.322,       dimPressure(), false, 0.0 },
    // Velocity
    { "m/s", 1.0,           dimVelocity(), false, 0.0 },
    { "km/h",1.0/3.6,       dimVelocity(), false, 0.0 },
    { "mph", 0.44704,       dimVelocity(), false, 0.0 },
    { "kn",  0.514444,      dimVelocity(), false, 0.0 },  // knot
    // Acceleration
    { "m/s2",1.0,           dimAccel(),    false, 0.0 },
    // Area
    { "m2",  1.0,           dimArea(),     false, 0.0 },
    { "cm2", 1e-4,          dimArea(),     false, 0.0 },
    { "km2", 1e6,           dimArea(),     false, 0.0 },
    { "ft2", 0.092903,      dimArea(),     false, 0.0 },
    { "ha",  1e4,           dimArea(),     false, 0.0 },
    // Volume
    { "m3",  1.0,           dimVolume(),   false, 0.0 },
    { "L",   0.001,         dimVolume(),   false, 0.0 },
    { "mL",  1e-6,          dimVolume(),   false, 0.0 },
    { "gal", 0.00378541,    dimVolume(),   false, 0.0 },
    // Charge
    { "C_",  1.0,           dimCharge(),   false, 0.0 },  // coulomb (C collides with Celsius)
    // Voltage
    { "V",   1.0,           dimVoltage(),  false, 0.0 },
    { "mV",  0.001,         dimVoltage(),  false, 0.0 },
    { "kV",  1000.0,        dimVoltage(),  false, 0.0 },
    // Frequency
    { "Hz",  1.0,           dimFrequency(),false, 0.0 },
    { "kHz", 1000.0,        dimFrequency(),false, 0.0 },
    { "MHz", 1e6,           dimFrequency(),false, 0.0 },
    // Dimensionless
    { "rad", 1.0,           dimNone(),     false, 0.0 },
    { "deg", M_PI/180.0, dimNone(), false, 0.0 },
};

const size_t NeoUnits::kUnitCount =
    sizeof(NeoUnits::kUnits) / sizeof(NeoUnits::kUnits[0]);

const NeoUnits::UnitInfo* NeoUnits::findUnit(const std::string& name) {
    for (size_t i = 0; i < kUnitCount; ++i) {
        if (name == kUnits[i].name) return &kUnits[i];
    }
    return nullptr;
}

// ════════════════════════════════════════════════════════════════════
// parseUnit
// ════════════════════════════════════════════════════════════════════

bool NeoUnits::parseUnit(const std::string& unitStr, NeoQuantity& out) {
    const UnitInfo* ui = findUnit(unitStr);
    if (!ui) return false;

    out.units    = ui->dims;
    out.symbol   = unitStr;
    // magnitude = SI conversion factor (value 1 of this unit in SI)
    out.magnitude = ui->factor;
    if (ui->hasOffset) out.magnitude = ui->factor;  // factor only; offset handled in makeQuantity
    return true;
}

bool NeoUnits::makeQuantity(double value, const std::string& unitStr,
                             NeoQuantity& out) {
    const UnitInfo* ui = findUnit(unitStr);
    if (!ui) return false;

    out.units  = ui->dims;
    out.symbol = unitStr;

    if (ui->hasOffset) {
        // Temperature: SI = factor * value + offset
        out.magnitude = ui->factor * value + ui->offset;
    } else {
        out.magnitude = ui->factor * value;
    }
    return true;
}

// ════════════════════════════════════════════════════════════════════
// convert
// ════════════════════════════════════════════════════════════════════

bool NeoUnits::convert(const NeoQuantity& qty,
                        const std::string& targetStr,
                        NeoQuantity&       out) {
    const UnitInfo* ui = findUnit(targetStr);
    if (!ui) return false;

    // Dimensional compatibility check
    if (qty.units != ui->dims) return false;

    out.units  = ui->dims;
    out.symbol = targetStr;

    if (ui->hasOffset) {
        // Temperature: value = (SI - offset) / factor
        out.magnitude = (qty.magnitude - ui->offset) / ui->factor;
        // Store as display value — the magnitude field for Qty holds SI when
        // created via makeQuantity, but for display we want the local value.
        // We'll store the display value here and mark it by setting a flag.
        // Convention: for temperature quantities, magnitude stores the SI value.
        // The display value is recomputed in toString().
        // Re-store SI:
        out.magnitude = qty.magnitude;
    } else {
        out.magnitude = qty.magnitude;  // still in SI
    }
    return true;
}

double NeoUnits::displayValue(const NeoQuantity& qty) {
    const UnitInfo* ui = findUnit(qty.symbol);
    if (!ui) return qty.magnitude;

    if (ui->hasOffset) {
        // Display value: (SI - offset) / factor
        return (qty.magnitude - ui->offset) / ui->factor;
    }
    if (std::fabs(ui->factor) < 1e-30) return qty.magnitude;
    return qty.magnitude / ui->factor;
}

// ════════════════════════════════════════════════════════════════════
// Arithmetic
// ════════════════════════════════════════════════════════════════════

bool NeoUnits::add(const NeoQuantity& a, const NeoQuantity& b, NeoQuantity& out) {
    if (a.units != b.units) return false;
    out.units     = a.units;
    out.magnitude = a.magnitude + b.magnitude;
    out.symbol    = a.symbol;  // Result uses first operand's unit label
    return true;
}

bool NeoUnits::sub(const NeoQuantity& a, const NeoQuantity& b, NeoQuantity& out) {
    if (a.units != b.units) return false;
    out.units     = a.units;
    out.magnitude = a.magnitude - b.magnitude;
    out.symbol    = a.symbol;
    return true;
}

void NeoUnits::mul(const NeoQuantity& a, const NeoQuantity& b, NeoQuantity& out) {
    out.magnitude = a.magnitude * b.magnitude;
    out.units     = a.units * b.units;
    // Build combined symbol
    if (a.symbol == "1" || a.symbol.empty()) {
        out.symbol = b.symbol;
    } else if (b.symbol == "1" || b.symbol.empty()) {
        out.symbol = a.symbol;
    } else {
        out.symbol = a.symbol + "\xC2\xB7" + b.symbol;  // UTF-8 '·'
    }
    // Check if it matches a named unit and rename
    // (optional: try to map combined UnitMap to a named derived unit)
    if (out.units == dimForce())    { out.symbol = "N"; }
    else if (out.units == dimEnergy())   { out.symbol = "J"; }
    else if (out.units == dimPower())    { out.symbol = "W"; }
    else if (out.units == dimPressure()) { out.symbol = "Pa"; }
    else if (out.units == dimVelocity()) { out.symbol = "m/s"; }
}

bool NeoUnits::div(const NeoQuantity& a, const NeoQuantity& b, NeoQuantity& out) {
    if (std::fabs(b.magnitude) < 1e-30) return false;
    out.magnitude = a.magnitude / b.magnitude;
    out.units     = a.units / b.units;
    if (b.symbol == "1" || b.symbol.empty()) {
        out.symbol = a.symbol;
    } else {
        out.symbol = a.symbol + "/" + b.symbol;
    }
    // Rename to named derived unit if applicable
    if (out.units == dimForce())    { out.symbol = "N"; }
    else if (out.units == dimEnergy())   { out.symbol = "J"; }
    else if (out.units == dimPower())    { out.symbol = "W"; }
    else if (out.units == dimPressure()) { out.symbol = "Pa"; }
    else if (out.units == dimVelocity()) { out.symbol = "m/s"; }
    else if (out.units.isDimensionless()) { out.symbol = ""; }
    return true;
}

void NeoUnits::scale(const NeoQuantity& a, double scalar, NeoQuantity& out) {
    out.magnitude = a.magnitude * scalar;
    out.units     = a.units;
    out.symbol    = a.symbol;
}

// ════════════════════════════════════════════════════════════════════
// toString
// ════════════════════════════════════════════════════════════════════

std::string NeoUnits::toString(const NeoQuantity& qty) {
    double disp = displayValue(qty);
    char buf[64];
    // Choose format: avoid exponent for numbers in a reasonable range
    if (std::fabs(disp) < 1e6 && std::fabs(disp) >= 1e-4 || disp == 0.0) {
        std::snprintf(buf, sizeof(buf), "%.6g", disp);
    } else {
        std::snprintf(buf, sizeof(buf), "%.4g", disp);
    }
    std::string result(buf);
    if (!qty.symbol.empty()) {
        result += " ";
        result += qty.symbol;
    }
    return result;
}
