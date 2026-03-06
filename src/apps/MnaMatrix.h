/**
 * MnaMatrix.h — Modified Nodal Analysis (MNA) Engine for CircuitCoreApp.
 *
 * Implements a 48-node SPICE-like circuit solver:
 *   - A·x = z  system (conductance + source stamps)
 *   - Gaussian elimination with partial pivoting
 *   - Parasitic 1e-9 conductance to ground (prevents singular matrices)
 *   - Union-Find algorithm for wire super-nodes
 *
 * Memory: matrices allocated in PSRAM via heap_caps_malloc.
 *
 * Part of: NumOS — Circuit Core Module
 */

#pragma once

#include <cstdint>
#include <cstring>

class MnaMatrix {
public:
    // ── Constants ────────────────────────────────────────────────────────
    static constexpr int MAX_NODES    = 48;
    static constexpr int MAX_VSOURCES = 16;
    /** Total matrix dimension: nodes-1 (node 0 = GND) + voltage sources */
    static constexpr int MAX_DIM      = (MAX_NODES - 1) + MAX_VSOURCES; // 63

    MnaMatrix();
    ~MnaMatrix();

    /** Allocate matrices in PSRAM.  Returns false on failure. */
    bool allocate();

    /** Free PSRAM allocations. */
    void deallocate();

    // ── Union-Find for wire super-nodes ──────────────────────────────────

    /** Reset all union-find parents to identity (each node = itself). */
    void ufReset();

    /** Merge two nodes (wire connection). */
    void ufUnion(int a, int b);

    /** Find the canonical representative of a node. */
    int ufFind(int n);

    // ── Stamping ─────────────────────────────────────────────────────────

    /** Clear matrices and re-apply parasitic conductance. */
    void stampClear();

    /** Stamp conductance between two nodes. node 0 = ground. */
    void stampConductance(int nodeA, int nodeB, float g);

    /** Stamp a resistor (convenience: g = 1/R). */
    void stampResistor(int nodeA, int nodeB, float resistance);

    /**
     * Stamp an ideal voltage source.
     * @param nodeP  positive terminal node
     * @param nodeN  negative terminal node
     * @param volts  voltage (P relative to N)
     * @param vsIdx  voltage source index (0..MAX_VSOURCES-1)
     */
    void stampVoltageSource(int nodeP, int nodeN, float volts, int vsIdx);

    /** Get/set the number of active voltage sources. */
    int  vsCount() const { return _vsCount; }
    void setVsCount(int n) { _vsCount = n; }

    // ── Transient analysis ──────────────────────────────────────────────

    /** Set the time step for transient analysis (seconds). */
    void setTimeStep(float dt) { _dt = dt; }
    float timeStep() const { return _dt; }

    /**
     * Stamp a capacitor companion model (Backward Euler).
     * Replaces C with: conductance G_eq = C/dt  and
     * current source I_eq = C/dt * V_prev.
     */
    void stampCapacitor(int nodeA, int nodeB, float capacitance, float vPrev);

    /**
     * Stamp a current source between two nodes.
     * Positive current flows from nodeA to nodeB.
     */
    void stampCurrentSource(int nodeA, int nodeB, float amps);

    // ── Solver ───────────────────────────────────────────────────────────

    /**
     * Solve A·x = z via Gaussian elimination with partial pivoting.
     * Results stored in x[] (solution vector).
     * @return true if solve succeeded (no singular matrix).
     */
    bool solve();

    /** Get the solved voltage at a node (after solve()). Node 0 = 0V. */
    float nodeVoltage(int node) const;

    /** Get the current through a voltage source (after solve()). */
    float vsCurrent(int vsIdx) const;

    /** Current matrix dimension (nodes-1 + vsCount). */
    int dimension() const { return _dim; }

private:
    // ── Matrix storage (PSRAM) ───────────────────────────────────────────
    float* _A;   // MAX_DIM * MAX_DIM
    float* _z;   // MAX_DIM
    float* _x;   // MAX_DIM (solution)

    int _dim;       // current dimension
    int _vsCount;   // active voltage sources
    float _dt;      // time step for transient analysis (seconds)

    // ── Union-Find parent array ──────────────────────────────────────────
    int _ufParent[MAX_NODES];

    // ── Internal helpers ─────────────────────────────────────────────────

    /** Map a raw node id to its matrix row (0-based, after UF mapping). */
    int nodeToRow(int node) const;

    /** Access A[row][col]. */
    float& A(int row, int col) { return _A[row * MAX_DIM + col]; }
    float  A(int row, int col) const { return _A[row * MAX_DIM + col]; }
};
