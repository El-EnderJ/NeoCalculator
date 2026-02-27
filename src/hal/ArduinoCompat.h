/**
 * ArduinoCompat.h — Arduino API compatibility for native (PC) builds
 *
 * Provee reemplazos mínimos de tipos y funciones de Arduino que el
 * código legacy referencia:  String, Serial, GPIO stubs, etc.
 *
 * Solo se activa cuando NO estamos en el framework Arduino.
 * Header-only (sin .cpp necesario).
 */

#pragma once

#ifndef ARDUINO   // ═══ Solo activo en builds no-Arduino ═══

#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <string>
#include <algorithm>

// ════════════════════════════════════════════════════════════════════════════
// Arduino String — Clase compatible mínima
// ════════════════════════════════════════════════════════════════════════════
class String {
public:
    String() {}
    String(const char* s) : _s(s ? s : "") {}
    String(const std::string& s) : _s(s) {}
    String(int val)          : _s(std::to_string(val)) {}
    String(unsigned int val) : _s(std::to_string(val)) {}
    String(long val)         : _s(std::to_string(val)) {}
    String(unsigned long val): _s(std::to_string(val)) {}
    String(double val, int decimals = 2) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.*f", decimals, val);
        _s = buf;
    }

    const char*  c_str()   const { return _s.c_str(); }
    unsigned int length()  const { return (unsigned int)_s.length(); }
    bool         isEmpty() const { return _s.empty(); }

    // ── Operadores ───────────────────────────────────────────────────
    String  operator+ (const String& o) const { return String(_s + o._s); }
    String  operator+ (const char* s)   const { return String(_s + (s ? s : "")); }
    String& operator+=(const String& o) { _s += o._s; return *this; }
    String& operator+=(const char* s)   { _s += (s ? s : ""); return *this; }
    String& operator+=(char c)          { _s += c; return *this; }
    bool    operator==(const String& o) const { return _s == o._s; }
    bool    operator!=(const String& o) const { return _s != o._s; }
    char    operator[](unsigned int i)  const { return _s[i]; }
    char&   operator[](unsigned int i)        { return _s[i]; }

    friend String operator+(const char* lhs, const String& rhs) {
        return String(std::string(lhs) + rhs._s);
    }

    // ── Métodos Arduino String ───────────────────────────────────────
    String substring(unsigned int from, unsigned int to) const {
        return String(_s.substr(from, to - from));
    }
    String substring(unsigned int from) const {
        return String(_s.substr(from));
    }
    int indexOf(char c, unsigned int from = 0) const {
        auto p = _s.find(c, from);
        return p == std::string::npos ? -1 : (int)p;
    }
    int indexOf(const String& sub, unsigned int from = 0) const {
        auto p = _s.find(sub._s, from);
        return p == std::string::npos ? -1 : (int)p;
    }
    int  toInt()    const { return std::atoi(_s.c_str()); }
    double toDouble() const { return std::atof(_s.c_str()); }
    float  toFloat()  const { return (float)std::atof(_s.c_str()); }

    void toLowerCase() { std::transform(_s.begin(), _s.end(), _s.begin(), ::tolower); }
    void toUpperCase() { std::transform(_s.begin(), _s.end(), _s.begin(), ::toupper); }
    void trim() {
        auto l = _s.find_first_not_of(" \t\r\n");
        auto r = _s.find_last_not_of(" \t\r\n");
        if (l == std::string::npos) _s.clear();
        else _s = _s.substr(l, r - l + 1);
    }

private:
    std::string _s;
};

// ════════════════════════════════════════════════════════════════════════════
// GPIO stubs
// ════════════════════════════════════════════════════════════════════════════
#ifndef INPUT
#define INPUT        0x0
#endif
#ifndef OUTPUT
#define OUTPUT       0x1
#endif
#ifndef INPUT_PULLUP
#define INPUT_PULLUP 0x2
#endif
#ifndef HIGH
#define HIGH         1
#endif
#ifndef LOW
#define LOW          0
#endif

inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int  digitalRead(int) { return HIGH; }
inline void analogWrite(int, int) {}

// ════════════════════════════════════════════════════════════════════════════
// Serial stub
// ════════════════════════════════════════════════════════════════════════════
class SerialCompat_ {
public:
    void begin(unsigned long) {}
    void print(const char* s)  { std::printf("%s", s); }
    void print(int v)          { std::printf("%d", v); }
    void print(unsigned v)     { std::printf("%u", v); }
    void print(double v)       { std::printf("%f", v); }
    void println()             { std::printf("\n"); }
    void println(const char* s){ std::printf("%s\n", s); }
    void println(int v)        { std::printf("%d\n", v); }
    void printf(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        std::vprintf(fmt, args);
        va_end(args);
    }
    void flush()     { std::fflush(stdout); }
    int  available() { return 0; }
    int  read()      { return -1; }
};

// No instanciamos aquí si el símbolo ya existe (e.g. incluido varias veces)
// En la práctica, cada TU ve su propia copia estática, lo cual es fine para un stub.
static SerialCompat_ Serial;

// ════════════════════════════════════════════════════════════════════════════
// Timing stubs (uso con SDL si está disponible, o chrono como fallback)
// ════════════════════════════════════════════════════════════════════════════
#ifdef NATIVE_SIM
    #include <SDL2/SDL.h>
    inline unsigned long millis()            { return SDL_GetTicks(); }
    inline void delay(unsigned long ms)      { SDL_Delay(ms); }
    inline unsigned long micros()            { return (unsigned long)SDL_GetTicks() * 1000UL; }
#else
    #include <chrono>
    #include <thread>
    inline unsigned long millis() {
        static auto t0 = std::chrono::steady_clock::now();
        return (unsigned long)std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0).count();
    }
    inline void delay(unsigned long ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
    inline unsigned long micros() { return millis() * 1000UL; }
#endif

// ════════════════════════════════════════════════════════════════════════════
// ESP32-specific stubs
// ════════════════════════════════════════════════════════════════════════════
inline bool psramFound()           { return false; }
inline void* ps_malloc(size_t sz)  { return malloc(sz); }

struct EspClass_ {
    uint32_t getPsramSize()  { return 0; }
    uint32_t getFreePsram()  { return 0; }
};
static EspClass_ ESP;

// PROGMEM (ya manejado en Constants.h, pero por si acaso)
#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const uint8_t*)(addr))
#endif

#endif // !ARDUINO
