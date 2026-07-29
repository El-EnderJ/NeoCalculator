/*
 * NeoCalculator - NumOS
 * Copyright (C) 2026 Juan Ramon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

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
#include <cstddef>

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

uint32_t VariableManager::checksum(const uint8_t* data,
                                   const std::size_t length) {
    uint32_t hash = 2166136261U;
    for (std::size_t i = 0; i < length; ++i) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    return hash;
}

bool VariableManager::validatePersistentValue(const ExactVal& value) {
    // These are representation invariants, not arbitrary numeric limits.
    // Large int64 numerators remain valid; poisoned denominators/radicands and
    // impossible symbolic exponents fall back only for their own slot.
    return value.den > 0 &&
           value.inner > 0 &&
           value.piMul >= -32 && value.piMul <= 32 &&
           value.eMul >= -32 && value.eMul <= 32;
}

// ════════════════════════════════════════════════════════════════════════════
// saveToFlash — Guarda todas las variables en LittleFS
// ════════════════════════════════════════════════════════════════════════════

bool VariableManager::saveToFlash() {
#if NUMOS_PRODUCTION_DEMO_PROFILE
    LittleFS.remove(TEMP_PATH);
    File f = LittleFS.open(TEMP_PATH, "w");
    if (!f) return false;

    bool complete = true;
    uint32_t magic = MAGIC;
    const uint8_t header[8] = {
        static_cast<uint8_t>(magic),
        static_cast<uint8_t>(magic >> 8U),
        static_cast<uint8_t>(magic >> 16U),
        static_cast<uint8_t>(magic >> 24U),
        FORMAT_VERSION,
        static_cast<uint8_t>(NUM_VARS),
        0,
        0,
    };
    complete = f.write(header, sizeof(header)) == sizeof(header);

    uint8_t buf[SLOT_RECORD_SIZE];
    for (int i = 0; i < NUM_VARS && complete; ++i) {
        serializeExactVal(buf, _vars[i]);
        const uint32_t crc = checksum(buf, EXACTVAL_SIZE);
        std::memcpy(buf + EXACTVAL_SIZE, &crc, sizeof(crc));
        complete = f.write(buf, sizeof(buf)) == sizeof(buf);
    }

    f.close();
    if (!complete) {
        LittleFS.remove(TEMP_PATH);
        return false;
    }
    LittleFS.remove(FLASH_PATH);
    return LittleFS.rename(TEMP_PATH, FLASH_PATH);
#else
    File f = LittleFS.open(FLASH_PATH, "w");
    if (!f) return false;
    uint32_t magic = MAGIC;
    f.write(reinterpret_cast<const uint8_t*>(&magic), 4);
    uint8_t count = NUM_VARS;
    f.write(&count, 1);
    uint8_t buf[EXACTVAL_SIZE];
    for (int i = 0; i < NUM_VARS; ++i) {
        serializeExactVal(buf, _vars[i]);
        f.write(buf, EXACTVAL_SIZE);
    }
    f.close();
    return true;
#endif
}

// ════════════════════════════════════════════════════════════════════════════
// loadFromFlash — Carga variables desde LittleFS
// ════════════════════════════════════════════════════════════════════════════

bool VariableManager::loadFromFlash() {
#if NUMOS_PRODUCTION_DEMO_PROFILE
    // Attempt open directly — avoids LittleFS.exists() which internally calls
    // open("r") and emits log_e via vfs_api.cpp:105 when the file is absent.
    File f = LittleFS.open(FLASH_PATH, "r");
    if (!f) {
        _lastLoadStatus = PersistentLoadStatus::Missing;
        return false;
    }

    resetAll();
    uint8_t header[8] = {};
    if (f.read(header, sizeof(header)) != sizeof(header)) {
        f.close();
        _lastLoadStatus = PersistentLoadStatus::Corrupt;
        return false;
    }
    uint32_t magic = 0;
    std::memcpy(&magic, header, sizeof(magic));
    if (magic != MAGIC) {
        f.close();
        _lastLoadStatus = PersistentLoadStatus::UnsupportedVersion;
        return false;
    }
    if (header[4] != FORMAT_VERSION || header[5] > NUM_VARS) {
        f.close();
        _lastLoadStatus = PersistentLoadStatus::UnsupportedVersion;
        return false;
    }

    const std::size_t count = header[5];
    const std::size_t expectedSize =
        sizeof(header) + count * SLOT_RECORD_SIZE;
    if (f.size() != expectedSize) {
        f.close();
        _lastLoadStatus = PersistentLoadStatus::Corrupt;
        return false;
    }

    bool partial = count != NUM_VARS;
    uint8_t buf[SLOT_RECORD_SIZE];
    for (std::size_t i = 0; i < count; ++i) {
        if (f.read(buf, sizeof(buf)) != sizeof(buf)) {
            partial = true;
            break;
        }
        uint32_t storedChecksum = 0;
        std::memcpy(&storedChecksum, buf + EXACTVAL_SIZE,
                    sizeof(storedChecksum));
        const ExactVal value = deserializeExactVal(buf);
        if (storedChecksum != checksum(buf, EXACTVAL_SIZE) ||
            !validatePersistentValue(value)) {
            partial = true;
            continue;
        }
        _vars[i] = value;
    }

    f.close();
    _lastLoadStatus = partial ? PersistentLoadStatus::Partial
                              : PersistentLoadStatus::Loaded;
    return true;
#else
    File f = LittleFS.open(FLASH_PATH, "r");
    if (!f) return false;
    uint32_t magic = 0;
    if (f.read(reinterpret_cast<uint8_t*>(&magic), 4) != 4 ||
        magic != MAGIC) {
        f.close();
        return false;
    }
    uint8_t count = 0;
    if (f.read(&count, 1) != 1) {
        f.close();
        return false;
    }
    const int toRead = count < NUM_VARS ? count : NUM_VARS;
    uint8_t buf[EXACTVAL_SIZE];
    for (int i = 0; i < toRead; ++i) {
        if (f.read(buf, EXACTVAL_SIZE) != EXACTVAL_SIZE) break;
        _vars[i] = deserializeExactVal(buf);
    }
    f.close();
    return true;
#endif
}

} // namespace vpam
