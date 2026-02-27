/**
 * Hal.h — Hardware Abstraction Layer
 *
 * Abstrae funciones que difieren entre ESP32 (Arduino) y PC (SDL2/native).
 * En código portable, usar HAL_Log/HAL_Delay/HAL_GetMillis en vez de
 * Serial.printf/delay/millis directamente.
 *
 * Definiciones:
 *   NATIVE_SIM  → Compilación nativa para PC (SDL2)
 *   ARDUINO     → Compilación para ESP32 (framework Arduino)
 */

#pragma once

#include <cstdint>

#ifdef NATIVE_SIM
    // ── PC: SDL2 + stdlib ──────────────────────────────────────────────
    #include <cstdio>
    #include <cstdarg>
    #include <SDL2/SDL.h>

    #define HAL_Log(fmt, ...)       printf("[SIM] " fmt "\n", ##__VA_ARGS__)
    #define HAL_Delay(ms)           SDL_Delay(ms)

    inline uint32_t HAL_GetMillis() { return SDL_GetTicks(); }

#elif defined(ARDUINO)
    // ── ESP32: Arduino framework ───────────────────────────────────────
    #include <Arduino.h>

    #define HAL_Log(fmt, ...)       Serial.printf(fmt "\n", ##__VA_ARGS__)
    #define HAL_Delay(ms)           delay(ms)

    inline uint32_t HAL_GetMillis() { return millis(); }

#else
    // ── Fallback: stdlib puro (sin SDL2 ni Arduino) ────────────────────
    #include <cstdio>
    #include <cstdarg>
    #include <chrono>
    #include <thread>

    #define HAL_Log(fmt, ...)       printf("[HAL] " fmt "\n", ##__VA_ARGS__)
    #define HAL_Delay(ms)           std::this_thread::sleep_for(std::chrono::milliseconds(ms))

    inline uint32_t HAL_GetMillis() {
        static auto t0 = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        return (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(now - t0).count();
    }

#endif
