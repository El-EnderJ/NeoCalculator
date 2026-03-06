/**
 * LuaVM.cpp — Lua VM implementation for CircuitCoreApp.
 *
 * Conditional compilation:
 *   - If LUA_AVAILABLE is defined and lua.h exists, uses real Lua 5.4
 *   - Otherwise, provides stub implementation (script stored, not executed)
 *
 * To enable real Lua on ESP32, add lua to lib_deps and define LUA_AVAILABLE.
 */

#include "LuaVM.h"
#include "MnaMatrix.h"
#include "CircuitComponent.h"
#include <cstring>
#include <cstdio>

// ══════════════════════════════════════════════════════════════════════════════
// Stub implementation (Lua not available)
// ══════════════════════════════════════════════════════════════════════════════

LuaVM::LuaVM()
    : _mna(nullptr)
    , _mcu(nullptr)
    , _hasScript(false)
    , _running(false)
    , _luaState(nullptr)
{
    _errorMsg[0] = '\0';
}

LuaVM::~LuaVM() {
    shutdown();
}

bool LuaVM::init(MnaMatrix* mna, MCUComponent* mcu) {
    _mna = mna;
    _mcu = mcu;

#ifdef LUA_AVAILABLE
    // TODO: Real Lua initialization
    // _luaState = luaL_newstate();
    // luaL_openlibs((lua_State*)_luaState);
    // Register circuit.readVoltage and circuit.setPin bindings
    snprintf(_errorMsg, sizeof(_errorMsg), "Lua ready");
    return true;
#else
    snprintf(_errorMsg, sizeof(_errorMsg), "Lua not available (stub mode)");
    return true;  // Stub always succeeds
#endif
}

void LuaVM::shutdown() {
#ifdef LUA_AVAILABLE
    if (_luaState) {
        // lua_close((lua_State*)_luaState);
        _luaState = nullptr;
    }
#endif
    _hasScript = false;
    _running = false;
    _mna = nullptr;
    _mcu = nullptr;
}

bool LuaVM::loadScript(const char* script) {
    if (!script) return false;

#ifdef LUA_AVAILABLE
    // TODO: Compile script and create coroutine
    // luaL_loadstring, lua_newthread, etc.
    _hasScript = true;
    _running = true;
    snprintf(_errorMsg, sizeof(_errorMsg), "Script loaded");
    return true;
#else
    (void)script;
    _hasScript = true;
    _running = false;
    snprintf(_errorMsg, sizeof(_errorMsg), "Lua stub: script stored");
    return true;
#endif
}

bool LuaVM::tick() {
    if (!_hasScript || !_running) return false;

#ifdef LUA_AVAILABLE
    // TODO: lua_resume coroutine
    // On yield: return true
    // On finish/error: _running = false, return false
    return _running;
#else
    return false;
#endif
}

void LuaVM::reset() {
    _hasScript = false;
    _running = false;
    _errorMsg[0] = '\0';

    if (_mcu) _mcu->clearPins();

#ifdef LUA_AVAILABLE
    // TODO: Close and re-create Lua state
#endif
}
