/**
 * VariableManager.cpp — Implementación del gestor global de variables
 *
 * Persistencia binaria en LittleFS (/vars.dat):
 *   Header: "VR01" (4 bytes)
 *   Count:  1 byte (número de variables)
 *   Data:   count * 34 bytes (ExactVal serializado)
 *
 * Cada ExactVal = num(8) + den(8) + outer(8) + inner(8) + piMul(1) + eMul(1)
 */

#include "VariableManager.h"

// ── LittleFS: solo incluir en ESP32 (Arduino) ──
#ifdef ARDUINO
#include <FS.h>
#include <LittleFS.h>
#else
#include "../hal/FileSystem.h"
#endif

#include <cstring>

namespace vpam {

// ════════════════════════════════════════════════════════════════════════════
// Singleton
// ════════════════════════════════════════════════════════════════════════════

VariableManager& VariableManager::instance() {
    static VariableManager inst;
    return inst;
}

// ════════════════════════════════════════════════════════════════════════════
// Constructor — Todas las variables inicializadas a 0
// ════════════════════════════════════════════════════════════════════════════

VariableManager::VariableManager() {
    resetAll();
}

// ════════════════════════════════════════════════════════════════════════════
// nameToIndex — Mapeo de char a índice interno
// ════════════════════════════════════════════════════════════════════════════

int VariableManager::nameToIndex(char name) {
    switch (name) {
        case 'A': return 0;
        case 'B': return 1;
        case 'C': return 2;
        case 'D': return 3;
        case 'E': return 4;
        case 'F': return 5;
        case 'x': return 6;
        case 'y': return 7;
        case 'z': return 8;
        case VAR_ANS:    return 9;
        case VAR_PREANS: return 10;
        default:  return -1;
    }
}

bool VariableManager::isValidName(char name) {
    return nameToIndex(name) >= 0;
}

// ════════════════════════════════════════════════════════════════════════════
// getVariable / setVariable
// ════════════════════════════════════════════════════════════════════════════

ExactVal VariableManager::getVariable(char name) const {
    int idx = nameToIndex(name);
    if (idx < 0) return ExactVal::fromInt(0);
    return _vars[idx];
}

void VariableManager::setVariable(char name, const ExactVal& val) {
    int idx = nameToIndex(name);
    if (idx < 0) return;
    _vars[idx] = val;
}

// ════════════════════════════════════════════════════════════════════════════
// updateAns — Rotación Ans → PreAns
// ════════════════════════════════════════════════════════════════════════════

void VariableManager::updateAns(const ExactVal& newVal) {
    _vars[nameToIndex(VAR_PREANS)] = _vars[nameToIndex(VAR_ANS)];
    _vars[nameToIndex(VAR_ANS)]    = newVal;
}

// ════════════════════════════════════════════════════════════════════════════
// resetAll — Reinicia todas las variables a 0
// ════════════════════════════════════════════════════════════════════════════

void VariableManager::resetAll() {
    for (auto& v : _vars) {
        v = ExactVal::fromInt(0);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// variableLabel — Texto legible para una variable
// ════════════════════════════════════════════════════════════════════════════

const char* VariableManager::variableLabel(char name) {
    switch (name) {
        case VAR_ANS:    return "Ans";
        case VAR_PREANS: return "PreAns";
        case 'A': return "A";
        case 'B': return "B";
        case 'C': return "C";
        case 'D': return "D";
        case 'E': return "E";
        case 'F': return "F";
        case 'x': return "x";
        case 'y': return "y";
        case 'z': return "z";
        default:  return "?";
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Serialización binaria de ExactVal
// ════════════════════════════════════════════════════════════════════════════

void VariableManager::serializeExactVal(uint8_t* buf, const ExactVal& val) const {
    // num (8 bytes, little-endian)
    std::memcpy(buf + 0,  &val.num,   sizeof(int64_t));
    std::memcpy(buf + 8,  &val.den,   sizeof(int64_t));
    std::memcpy(buf + 16, &val.outer, sizeof(int64_t));
    std::memcpy(buf + 24, &val.inner, sizeof(int64_t));
    buf[32] = static_cast<uint8_t>(val.piMul);
    buf[33] = static_cast<uint8_t>(val.eMul);
}

ExactVal VariableManager::deserializeExactVal(const uint8_t* buf) const {
    ExactVal val;
    std::memcpy(&val.num,   buf + 0,  sizeof(int64_t));
    std::memcpy(&val.den,   buf + 8,  sizeof(int64_t));
    std::memcpy(&val.outer, buf + 16, sizeof(int64_t));
    std::memcpy(&val.inner, buf + 24, sizeof(int64_t));
    val.piMul = static_cast<int8_t>(buf[32]);
    val.eMul  = static_cast<int8_t>(buf[33]);
    val.ok    = true;
    return val;
}

// ════════════════════════════════════════════════════════════════════════════
// saveToFlash — Guarda todas las variables en LittleFS
// ════════════════════════════════════════════════════════════════════════════

bool VariableManager::saveToFlash() {
    File f = LittleFS.open(FLASH_PATH, "w");
    if (!f) return false;

    // Magic "VR01"
    uint32_t magic = MAGIC;
    f.write(reinterpret_cast<const uint8_t*>(&magic), 4);

    // Count
    uint8_t count = NUM_VARS;
    f.write(&count, 1);

    // Variables
    uint8_t buf[EXACTVAL_SIZE];
    for (int i = 0; i < NUM_VARS; ++i) {
        serializeExactVal(buf, _vars[i]);
        f.write(buf, EXACTVAL_SIZE);
    }

    f.close();
    return true;
}

// ════════════════════════════════════════════════════════════════════════════
// loadFromFlash — Carga variables desde LittleFS
// ════════════════════════════════════════════════════════════════════════════

bool VariableManager::loadFromFlash() {
    if (!LittleFS.exists(FLASH_PATH)) return false;

    File f = LittleFS.open(FLASH_PATH, "r");
    if (!f) return false;

    // Verificar magic
    uint32_t magic = 0;
    if (f.read(reinterpret_cast<uint8_t*>(&magic), 4) != 4 || magic != MAGIC) {
        f.close();
        return false;
    }

    // Count
    uint8_t count = 0;
    if (f.read(&count, 1) != 1) {
        f.close();
        return false;
    }

    // Leer solo hasta NUM_VARS, ignorar extras
    int toRead = (count < NUM_VARS) ? count : NUM_VARS;
    uint8_t buf[EXACTVAL_SIZE];
    for (int i = 0; i < toRead; ++i) {
        if (f.read(buf, EXACTVAL_SIZE) != EXACTVAL_SIZE) break;
        _vars[i] = deserializeExactVal(buf);
    }

    f.close();
    return true;
}

} // namespace vpam
