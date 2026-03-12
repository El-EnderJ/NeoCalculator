/**
 * NeoIO.cpp — Platform-specific file I/O implementation for NeoLanguage Phase 7.
 *
 * On Arduino/ESP32 targets: uses LittleFS.
 * On all other targets: uses standard C stdio.
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage Phase 7
 */

// Include platform headers FIRST, before any NeoLanguage headers,
// to avoid name collisions between Arduino macros and NeoLanguage symbols.

#if defined(ARDUINO)
  #include <LittleFS.h>
  #define NEO_IO_USE_LITTLEFS 1
#else
  #include <cstdio>
  #define NEO_IO_USE_LITTLEFS 0
#endif

// NeoIO.h must be included AFTER the platform headers so that any
// Arduino macro pollution (e.g. bitToggle) is already in scope and
// doesn't interfere with the NeoLanguage headers that follow.
#include "NeoIO.h"

namespace NeoIO {

// ════════════════════════════════════════════════════════════════════
// File-handle pool
// ════════════════════════════════════════════════════════════════════

struct FileSlot {
    bool open = false;
#if NEO_IO_USE_LITTLEFS
    File handle;
#else
    FILE* handle = nullptr;
#endif
};

static FileSlot g_slots[NEO_IO_MAX_FILES];

// ════════════════════════════════════════════════════════════════════
// open / read / write / close
// ════════════════════════════════════════════════════════════════════

int openFile(const char* path, const char* mode) {
    for (int i = 0; i < NEO_IO_MAX_FILES; ++i) {
        if (!g_slots[i].open) {
#if NEO_IO_USE_LITTLEFS
            const char* m = "r";
            if (mode[0] == 'w') m = "w";
            else if (mode[0] == 'a') m = "a";
            g_slots[i].handle = LittleFS.open(path, m);
            if (!g_slots[i].handle) return -1;
#else
            g_slots[i].handle = std::fopen(path, mode);
            if (!g_slots[i].handle) return -1;
#endif
            g_slots[i].open = true;
            return i;
        }
    }
    return -1;  // No free slots
}

std::string readFile(int idx) {
    if (idx < 0 || idx >= NEO_IO_MAX_FILES || !g_slots[idx].open) return "";
    std::string out;
#if NEO_IO_USE_LITTLEFS
    File& f = g_slots[idx].handle;
    while (f.available()) {
        out += static_cast<char>(f.read());
    }
#else
    FILE* f = g_slots[idx].handle;
    if (!f) return "";
    char buf[256];
    while (std::fgets(buf, sizeof(buf), f)) out += buf;
#endif
    return out;
}

int writeFile(int idx, const char* text) {
    if (idx < 0 || idx >= NEO_IO_MAX_FILES || !g_slots[idx].open) return -1;
    int n = static_cast<int>(std::strlen(text));
#if NEO_IO_USE_LITTLEFS
    return static_cast<int>(g_slots[idx].handle.write(
        reinterpret_cast<const uint8_t*>(text), static_cast<size_t>(n)));
#else
    return static_cast<int>(std::fwrite(text, 1, static_cast<size_t>(n),
                                         g_slots[idx].handle));
#endif
}

void closeFile(int idx) {
    if (idx < 0 || idx >= NEO_IO_MAX_FILES || !g_slots[idx].open) return;
#if NEO_IO_USE_LITTLEFS
    g_slots[idx].handle.close();
#else
    std::fclose(g_slots[idx].handle);
    g_slots[idx].handle = nullptr;
#endif
    g_slots[idx].open = false;
}

} // namespace NeoIO
