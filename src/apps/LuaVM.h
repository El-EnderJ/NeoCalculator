/**
 * LuaVM.h — Lua 5.4 Virtual Microcontroller wrapper for CircuitCoreApp.
 *
 * Provides a coroutine-based Lua execution environment that:
 *   - Runs user scripts as coroutines (yielding back to C++ each tick)
 *   - Exposes circuit.readVoltage(nodeId) and circuit.setPin(pinIdx, voltage)
 *   - Integrates with the MNA engine at 30Hz
 *
 * When Lua is not available (no lua.h), provides a stub implementation
 * that stores a script but doesn't execute it.
 *
 * Part of: NumOS — Circuit Core Module
 */

#pragma once

#include <cstdint>

// Forward declarations
class MnaMatrix;
class MCUComponent;

class LuaVM {
public:
    LuaVM();
    ~LuaVM();

    /**
     * Initialize the Lua state.
     * @param mna    pointer to MNA engine (for readVoltage binding)
     * @param mcu    pointer to MCU component (for setPin binding)
     * @return true if Lua initialized successfully
     */
    bool init(MnaMatrix* mna, MCUComponent* mcu);

    /** Shut down Lua state and free memory. */
    void shutdown();

    /**
     * Load a Lua script (stored as coroutine).
     * @param script  null-terminated Lua source code
     * @return true if script compiled successfully
     */
    bool loadScript(const char* script);

    /**
     * Resume the Lua coroutine for one tick.
     * Call this at 30Hz (every other frame).
     * @return true if coroutine is still alive, false if finished/errored
     */
    bool tick();

    /** Check if the VM has a loaded script. */
    bool hasScript() const { return _hasScript; }

    /** Check if the VM is currently running. */
    bool isRunning() const { return _running; }

    /** Get the last error message (if any). */
    const char* lastError() const { return _errorMsg; }

    /** Reset the VM (stop script, clear state). */
    void reset();

private:
    MnaMatrix*    _mna;
    MCUComponent* _mcu;
    bool          _hasScript;
    bool          _running;
    char          _errorMsg[64];

    // Lua state is opaque — actual implementation depends on lua.h availability
    void*         _luaState;
};
