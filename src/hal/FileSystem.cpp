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

#ifdef _WIN32
    #include <direct.h>
    #define MKDIR_P(path) _mkdir(path)
#else
    #include <sys/stat.h>
    #define MKDIR_P(path) mkdir(path, 0755)
#endif

static constexpr const char* EMULATOR_DATA_DIR = "./emulator_data";

// ── Instancia global ────────────────────────────────────────────────────────
LittleFSClass LittleFS;

// ── fullPath — Convierte ruta LittleFS a ruta local del PC ──────────────────
std::string LittleFSClass::fullPath(const char* path) const {
    std::string p = EMULATOR_DATA_DIR;
    if (path && path[0] != '/' && path[0] != '\\') p += '/';
    if (path) p += path;
    return p;
}

// ── begin — Crea el directorio emulator_data si no existe ───────────────────
bool LittleFSClass::begin(bool /*formatOnFail*/) {
    MKDIR_P(EMULATOR_DATA_DIR);
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
    std::string bmode = mode;
    if (bmode.find('b') == std::string::npos) {
        bmode += 'b';
    }

    std::string fp = fullPath(path);
    FILE* f = std::fopen(fp.c_str(), bmode.c_str());
    return File(f);
}

// ── remove — Elimina un archivo ─────────────────────────────────────────────
bool LittleFSClass::remove(const char* path) {
    std::string fp = fullPath(path);
    return std::remove(fp.c_str()) == 0;
}

#endif // !ARDUINO
