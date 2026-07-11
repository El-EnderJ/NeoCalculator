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
 * FileSystem.h — Wrapper LittleFS-compatible para builds nativos (PC)
 *
 * En modo NATIVE_SIM provee las clases File y LittleFSClass que emulan
 * la API de LittleFS del framework Arduino, usando el sistema de archivos
 * local del PC con carpeta base ./emulator_data/.
 *
 * Uso en código que ya usa LittleFS:
 *   #ifdef ARDUINO
 *   #include <FS.h>
 *   #include <LittleFS.h>
 *   #else
 *   #include "hal/FileSystem.h"
 *   #endif
 *
 * El código de serialización (File::write, File::read, LittleFS.open, etc.)
 * funciona sin cambios en ambas plataformas.
 */

#pragma once

#ifndef ARDUINO   // ═══ Solo activo en builds no-Arduino ═══

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>

// ════════════════════════════════════════════════════════════════════════════
// File — Wrapper sobre FILE* compatible con Arduino File
// ════════════════════════════════════════════════════════════════════════════
class File {
public:
    File()         : _fp(nullptr) {}
    File(FILE* fp) : _fp(fp) {}
    ~File()        { close(); }

    // Move semantics (Arduino File es copiable, pero para native mover es suficiente)
    File(File&& other) noexcept : _fp(other._fp) { other._fp = nullptr; }
    File& operator=(File&& other) noexcept {
        if (this != &other) { close(); _fp = other._fp; other._fp = nullptr; }
        return *this;
    }

    explicit operator bool() const { return _fp != nullptr; }

    void close() {
        if (_fp) { std::fclose(_fp); _fp = nullptr; }
    }

    size_t write(const uint8_t* buf, size_t len) {
        if (!_fp) return 0;
        return std::fwrite(buf, 1, len, _fp);
    }

    /// Lee hasta len bytes en buf.  Devuelve bytes leídos.
    size_t read(uint8_t* buf, size_t len) {
        if (!_fp) return 0;
        return std::fread(buf, 1, len, _fp);
    }

    /// Tamaño total del archivo (seek al final y volver)
    size_t size() {
        if (!_fp) return 0;
        long cur = std::ftell(_fp);
        std::fseek(_fp, 0, SEEK_END);
        long sz = std::ftell(_fp);
        std::fseek(_fp, cur, SEEK_SET);
        return (size_t)(sz > 0 ? sz : 0);
    }

    /// Posición actual del cursor
    size_t position() {
        if (!_fp) return 0;
        long p = std::ftell(_fp);
        return (size_t)(p > 0 ? p : 0);
    }

    size_t available() {
        return size() - position();
    }

    void seek(size_t pos) {
        if (_fp) std::fseek(_fp, (long)pos, SEEK_SET);
    }

private:
    FILE* _fp;

    // No copyable
    File(const File&) = delete;
    File& operator=(const File&) = delete;
};

// ════════════════════════════════════════════════════════════════════════════
// LittleFSClass — Emulación de LittleFS que usa ./emulator_data/
// ════════════════════════════════════════════════════════════════════════════
class LittleFSClass {
public:
    /**
     * Fija el directorio raíz del sistema de archivos emulado (FIX-01).
     * Debe llamarse ANTES de begin(); por defecto "./emulator_data".
     * Permite a NativeHal apuntar la persistencia a un sandbox por-ejecución
     * (runs deterministas/CI) sin tocar el árbol del repositorio.
     */
    static void setRoot(const char* root);

    /** Directorio raíz actualmente configurado. */
    static const char* root();

    /**
     * Inicializa el "sistema de archivos" (crea el directorio base).
     * @param formatOnFail  Ignorado en PC (siempre crea el directorio).
     */
    bool begin(bool formatOnFail = false);

    /** Verifica si un archivo existe. */
    bool exists(const char* path);

    /**
     * Abre un archivo.
     * @param path  Ruta absoluta (ej: "/vars.dat")
     * @param mode  "r" (lectura) o "w" (escritura/creación)
     */
    File open(const char* path, const char* mode);

    /** Elimina un archivo. */
    bool remove(const char* path);

private:
    std::string fullPath(const char* path) const;
    bool _initialized = false;
};

// ── Instancia global (como en Arduino) ──
extern LittleFSClass LittleFS;

#endif // !ARDUINO
