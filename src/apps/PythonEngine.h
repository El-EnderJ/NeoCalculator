/**
 * PythonEngine.h — MicroPython Wrapper / Mock Engine
 *
 * Provides a simulated Python execution environment for the IDE.
 * Validates basic syntax and executes simple Python constructs
 * (print, for loops, math expressions, variable assignments).
 *
 * On ESP32-S3, the heap is allocated from PSRAM.
 * Stdout/stderr are captured to a circular buffer for the UI console.
 */
#pragma once

#include <cstdint>
#include <cstring>

class PythonEngine {
public:
    PythonEngine();
    ~PythonEngine();

    /// Initialize the engine (allocate PSRAM heap)
    void init();

    /// Shut down the engine (free PSRAM heap)
    void deinit();

    /// Execute a Python script string. Returns true on success.
    bool execute(const char* code);

    /// Get captured stdout output after execution
    const char* getOutput() const { return _outBuf; }

    /// Get captured stderr output (error messages)
    const char* getError() const { return _errBuf; }

    /// Clear output buffers
    void clearOutput();

    /// Check if the engine has been initialized
    bool isReady() const { return _initialized; }

private:
    static constexpr int OUT_BUF_SIZE = 2048;
    static constexpr int ERR_BUF_SIZE = 512;
    static constexpr int HEAP_SIZE    = 1 * 1024 * 1024;  // 1 MB PSRAM
    static constexpr int HEAP_FALLBACK = 256 * 1024;      // 256 KB fallback

    char  _outBuf[OUT_BUF_SIZE];
    int   _outPos;
    char  _errBuf[ERR_BUF_SIZE];
    int   _errPos;
    void* _heap;
    bool  _initialized;

    /// Append text to the output buffer
    void appendOut(const char* text);

    /// Append text to the error buffer
    void appendErr(const char* text);

    /// Simple line-by-line interpreter
    bool interpretLine(const char* line);

    /// Evaluate a simple math expression string to a double
    bool evalExpr(const char* expr, double& result);

    /// Check if a string looks like a valid Python identifier
    static bool isIdentifier(const char* s, int len);
};
