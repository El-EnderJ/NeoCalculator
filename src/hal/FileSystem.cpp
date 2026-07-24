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
 * FileSystem.cpp — Implementación del wrapper LittleFS para PC
 *
 * Simula LittleFS creando archivos en ./emulator_data/ usando
 * funciones estándar de C (fopen, fwrite, fread).
 *
 * Solo se compila cuando NATIVE_SIM está definido.
 */

#ifndef ARDUINO

#include "FileSystem.h"
#include <cstdio>

#ifdef __EMSCRIPTEN__
    #include <emscripten.h>

EM_JS(void, numosFilesystemDidMutate, (int operation), {
    const callback = Module['numosPersistenceDirty'];
    if (typeof callback === 'function') callback(operation);
});
#endif

#ifdef _WIN32
    #include <direct.h>
    #define MKDIR_P(path) ::_mkdir(path)
#else
    #include <sys/stat.h>
    #define MKDIR_P(path) ::mkdir(path, 0755)
#endif

// Raíz configurable (FIX-01): por defecto el comportamiento histórico
// ./emulator_data (relativo al CWD); NativeHal puede redirigirla a un
// sandbox por-ejecución ANTES de begin().
static std::string s_rootDir = "./emulator_data";

// ── Instancia global ────────────────────────────────────────────────────────
LittleFSClass LittleFS;

// ── setRoot / root — raíz configurable del filesystem emulado ───────────────
void LittleFSClass::setRoot(const char* root) {
    if (root && root[0]) s_rootDir = root;
}

const char* LittleFSClass::root() {
    return s_rootDir.c_str();
}

// ── fullPath — Convierte ruta LittleFS a ruta local del PC ──────────────────
std::string LittleFSClass::fullPath(const char* path) const {
    std::string p = s_rootDir;
    if (path && path[0] != '/' && path[0] != '\\') p += '/';
    if (path) p += path;
    return p;
}

// ── begin — Crea el directorio raíz si no existe ────────────────────────────
bool LittleFSClass::begin(bool /*formatOnFail*/) {
    MKDIR_P(s_rootDir.c_str());
    _initialized = true;
    return true;
}

// ── exists — Verifica si el archivo existe ──────────────────────────────────
bool LittleFSClass::exists(const char* path) {
    std::string fp = fullPath(path);
    FILE* f = std::fopen(fp.c_str(), "rb");
    if (f) {
        std::fclose(f);
        return true;
    }
    return false;
}

// ── open — Abre o crea un archivo ───────────────────────────────────────────
File LittleFSClass::open(const char* path, const char* mode) {
    if (!_initialized) return File();

    // Forzar modo binario (LittleFS siempre es binario)
    std::string bmode = mode ? mode : "r";
    if (bmode.find('b') == std::string::npos) {
        bmode += 'b';
    }

    std::string fp = fullPath(path);
    FILE* f = std::fopen(fp.c_str(), bmode.c_str());
    const bool mutating = bmode.find('w') != std::string::npos ||
                          bmode.find('a') != std::string::npos ||
                          bmode.find('+') != std::string::npos;
    return File(f, mutating);
}

// ── remove — Elimina un archivo ─────────────────────────────────────────────
bool LittleFSClass::remove(const char* path) {
    std::string fp = fullPath(path);
    const bool removed = std::remove(fp.c_str()) == 0;
#ifdef __EMSCRIPTEN__
    if (removed) numosFilesystemDidMutate(2);
#endif
    return removed;
}

// ── rename — Renombra dentro de la raíz emulada ─────────────────────────────
bool LittleFSClass::rename(const char* from, const char* to) {
    const std::string source = fullPath(from);
    const std::string destination = fullPath(to);
    const bool renamed = std::rename(source.c_str(), destination.c_str()) == 0;
#ifdef __EMSCRIPTEN__
    if (renamed) numosFilesystemDidMutate(4);
#endif
    return renamed;
}

// ── mkdir — Crea un directorio dentro de la raíz emulada ────────────────────
bool LittleFSClass::mkdir(const char* path) {
    const std::string fp = fullPath(path);
    const bool created = MKDIR_P(fp.c_str()) == 0;
#ifdef __EMSCRIPTEN__
    if (created) numosFilesystemDidMutate(3);
#endif
    return created;
}

#endif // !ARDUINO
