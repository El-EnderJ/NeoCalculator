/**
 * VariableContext.h
 * Gestión de variables A-Z y Ans con persistencia en NVS (Preferences).
 */

#pragma once

#include <Arduino.h>
#include <Preferences.h>

class VariableContext {
public:
    VariableContext();

    // En Wokwi, NVS funciona correctamente, pero a veces requiere un pequeño delay inicial
    // para estabilizar la partición virtual.
    void begin() {
        #ifdef WOKWI_SIMULATOR
            delay(100);
        #endif
        _prefs.begin(NVS_NAMESPACE, false);
        loadFromNVS();
    }
    void loadFromNVS();
    void saveToNVS();

    bool setVariable(char name, double value);
    void setVar(char name, double value) { setVariable(name, value); }
    bool getVariable(char name, double &outValue) const;

    void setAns(double value);
    double getAns() const;

private:
    struct PackedVars {
        uint32_t magic;
        double vars[26];
        uint8_t has[26];
        double ans;
    };

    static constexpr uint32_t MAGIC = 0xC41234;
    static constexpr const char *NVS_NAMESPACE = "calcVars";
    static constexpr const char *NVS_KEY = "vars";

    mutable Preferences _prefs;
    PackedVars _data;

    int indexFromName(char name) const;
};

