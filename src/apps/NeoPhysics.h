/**
 * NeoPhysics.h — Physics Constants Database for NeoLanguage Phase 7.
 *
 * Provides the const(name) built-in function that returns the numerical
 * value of a fundamental physical or mathematical constant.
 *
 * Contains 50+ NIST / CODATA 2018 values:
 *   Speed of light, Planck, Boltzmann, Avogadro, elementary charge,
 *   gravitational constant, electron/proton/neutron masses,
 *   vacuum permittivity/permeability, fine-structure constant,
 *   Rydberg, Bohr radius, Compton wavelength, magnetic flux quantum,
 *   gas constant, Faraday, Stefan–Boltzmann, Wien displacement,
 *   standard atmosphere, molar mass of electron, proton, neutron, etc.
 *
 * Usage in NeoLanguage:
 *   c = const("c")          # 2.997924458e8 m/s
 *   h = const("h")          # 6.62607015e-34 J·s
 *   print(const("k_B"))     # 1.380649e-23 J/K
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage Phase 7
 */
#pragma once

#include <cstring>
#include <cmath>
#include <limits>
#include "NeoValue.h"

namespace NeoPhysics {

// ════════════════════════════════════════════════════════════════════
// Constants table
// ════════════════════════════════════════════════════════════════════

struct PhysicsConstant {
    const char*  name;     ///< Lookup name (case-sensitive)
    double       value;    ///< SI numerical value
    const char*  unit;     ///< Human-readable unit string
    const char*  desc;     ///< Short description
};

static const PhysicsConstant PHYSICS_TABLE[] = {
    // ── Fundamental electromagnetic ──────────────────────────────
    { "c",        2.99792458e8,     "m/s",        "Speed of light in vacuum"            },
    { "c0",       2.99792458e8,     "m/s",        "Speed of light in vacuum (alt.)"     },
    { "mu_0",     1.25663706212e-6, "N/A²",       "Vacuum magnetic permeability"        },
    { "eps_0",    8.8541878128e-12, "F/m",        "Vacuum electric permittivity"        },
    { "k_e",      8.9875517923e9,   "N·m²/C²",    "Coulomb constant"                    },
    { "e",        1.602176634e-19,  "C",          "Elementary charge"                   },
    { "q_e",      1.602176634e-19,  "C",          "Elementary charge (alt.)"            },
    { "phi_0",    2.067833848e-15,  "Wb",         "Magnetic flux quantum"               },
    { "R_K",      25812.80745,      "Ohm",        "von Klitzing constant"               },
    { "alpha",    7.2973525693e-3,  "",           "Fine-structure constant"             },

    // ── Planck / quantum ─────────────────────────────────────────
    { "h",        6.62607015e-34,   "J·s",        "Planck constant"                     },
    { "hbar",     1.054571817e-34,  "J·s",        "Reduced Planck constant ℏ"           },
    { "h_bar",    1.054571817e-34,  "J·s",        "Reduced Planck constant (alt.)"      },

    // ── Thermodynamic ─────────────────────────────────────────────
    { "k_B",      1.380649e-23,     "J/K",        "Boltzmann constant"                  },
    { "k_b",      1.380649e-23,     "J/K",        "Boltzmann constant (alt.)"           },
    { "R",        8.314462618,      "J/(mol·K)",  "Molar gas constant"                  },
    { "N_A",      6.02214076e23,    "1/mol",      "Avogadro constant"                   },
    { "sigma_SB", 5.670374419e-8,   "W/(m²·K⁴)", "Stefan-Boltzmann constant"           },
    { "b_W",      2.897771955e-3,   "m·K",        "Wien displacement constant"          },

    // ── Gravitation & mechanics ───────────────────────────────────
    { "G",        6.67430e-11,      "m³/(kg·s²)", "Newtonian gravitational constant"    },
    { "g",        9.80665,          "m/s²",       "Standard gravity"                    },
    { "atm",      101325.0,         "Pa",         "Standard atmosphere"                 },

    // ── Particle masses ───────────────────────────────────────────
    { "m_e",      9.1093837015e-31, "kg",         "Electron rest mass"                  },
    { "m_p",      1.67262192369e-27,"kg",         "Proton rest mass"                    },
    { "m_n",      1.67492749804e-27,"kg",         "Neutron rest mass"                   },
    { "m_u",      1.66053906660e-27,"kg",         "Atomic mass unit"                    },
    { "u",        1.66053906660e-27,"kg",         "Atomic mass unit (alt.)"             },

    // ── Atomic / Bohr ─────────────────────────────────────────────
    { "a_0",      5.29177210903e-11,"m",          "Bohr radius"                         },
    { "R_inf",    1.0973731568160e7,"1/m",        "Rydberg constant"                    },
    { "lambda_C", 2.42631023867e-12,"m",          "Compton wavelength (electron)"       },
    { "r_e",      2.8179403262e-15, "m",          "Classical electron radius"           },

    // ── Electrochemistry ─────────────────────────────────────────
    { "F",        96485.33212,      "C/mol",      "Faraday constant"                    },

    // ── Radiation / photon ───────────────────────────────────────
    { "eV",       1.602176634e-19,  "J",          "Electron-volt"                       },
    { "keV",      1.602176634e-16,  "J",          "Kilo-electron-volt"                  },
    { "MeV",      1.602176634e-13,  "J",          "Mega-electron-volt"                  },

    // ── Magnetic moments ─────────────────────────────────────────
    { "mu_B",     9.2740100783e-24, "J/T",        "Bohr magneton"                       },
    { "mu_N",     5.0507837461e-27, "J/T",        "Nuclear magneton"                    },

    // ── Mathematical constants ────────────────────────────────────
    { "pi",       3.14159265358979323846, "",     "Pi"                                  },
    { "tau",      6.28318530717958647692, "",     "Tau (2π)"                            },
    { "euler",    2.71828182845904523536, "",     "Euler's number e"                    },
    { "phi_g",    1.61803398874989484820, "",     "Golden ratio φ"                      },
    { "sqrt2",    1.41421356237309504880, "",     "√2"                                  },
    { "sqrt3",    1.73205080756887729353, "",     "√3"                                  },
    { "ln2",      0.69314718055994530942, "",     "ln(2)"                               },

    // ── Astronomy ─────────────────────────────────────────────────
    { "AU",       1.495978707e11,   "m",          "Astronomical unit"                   },
    { "ly",       9.4607304725808e15,"m",         "Light-year"                          },
    { "pc",       3.085677581e16,   "m",          "Parsec"                              },
    { "M_sun",    1.989e30,         "kg",         "Solar mass"                          },
    { "R_sun",    6.957e8,          "m",          "Solar radius"                        },
    { "L_sun",    3.828e26,         "W",          "Solar luminosity"                    },
    { "M_earth",  5.9722e24,        "kg",         "Earth mass"                          },
    { "R_earth",  6.3710e6,         "m",          "Earth equatorial radius"             },

    // ── Thermochemistry ──────────────────────────────────────────
    { "cal",      4.184,            "J",          "Thermochemical calorie"              },
    { "kcal",     4184.0,           "J",          "Thermochemical kilocalorie"          },
    { "kWh",      3.6e6,            "J",          "Kilowatt-hour"                       },
};

static constexpr int PHYSICS_TABLE_SIZE =
    static_cast<int>(sizeof(PHYSICS_TABLE) / sizeof(PHYSICS_TABLE[0]));

// ════════════════════════════════════════════════════════════════════
// Lookup function
// ════════════════════════════════════════════════════════════════════

/**
 * Look up a physics constant by name.
 * @param name  Case-sensitive constant name (e.g. "c", "k_B", "m_e").
 * @param value [out] Numerical value in SI units.
 * @return true if found, false otherwise.
 */
inline bool lookup(const char* name, double& value) {
    for (int i = 0; i < PHYSICS_TABLE_SIZE; ++i) {
        if (std::strcmp(PHYSICS_TABLE[i].name, name) == 0) {
            value = PHYSICS_TABLE[i].value;
            return true;
        }
    }
    return false;
}

/**
 * Return a descriptive string for a constant (name: value unit — desc).
 * Returns empty string if not found.
 */
inline std::string describe(const char* name) {
    for (int i = 0; i < PHYSICS_TABLE_SIZE; ++i) {
        if (std::strcmp(PHYSICS_TABLE[i].name, name) == 0) {
            const auto& c = PHYSICS_TABLE[i];
            char buf[256];
            std::snprintf(buf, sizeof(buf), "%s = %.6g %s — %s",
                          c.name, c.value, c.unit, c.desc);
            return std::string(buf);
        }
    }
    return "";
}

/**
 * Convenience: return NeoValue::makeNumber(value) for the constant,
 * or NeoValue::makeNull() if not found.
 */
inline NeoValue get(const char* name) {
    double v = 0.0;
    if (lookup(name, v)) return NeoValue::makeNumber(v);
    return NeoValue::makeNull();
}

} // namespace NeoPhysics
