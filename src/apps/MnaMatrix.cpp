/**
 * MnaMatrix.cpp — Modified Nodal Analysis engine implementation.
 *
 * Gaussian elimination with partial pivoting.
 * Parasitic 1 nS conductance on every node to ground.
 * Union-Find for wire super-nodes.
 */

#include "MnaMatrix.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

#ifdef ARDUINO
  #include <esp_heap_caps.h>
#endif

// ── Parasitic conductance (1 nS = 1e-9 S) ──────────────────────────────────
static constexpr float PARASITIC_G = 1e-9f;

// ══ Constructor / Destructor ═════════════════════════════════════════════════

MnaMatrix::MnaMatrix()
    : _A(nullptr), _z(nullptr), _x(nullptr)
    , _dim(MAX_NODES - 1), _vsCount(0), _dt(1.0f / 30.0f)
{
    ufReset();
}

MnaMatrix::~MnaMatrix() {
    deallocate();
}

// ══ Memory management ═══════════════════════════════════════════════════════

bool MnaMatrix::allocate() {
    if (_A) return true;  // already allocated

    size_t matSize = MAX_DIM * MAX_DIM * sizeof(float);
    size_t vecSize = MAX_DIM * sizeof(float);

#ifdef ARDUINO
    _A = (float*)heap_caps_malloc(matSize, MALLOC_CAP_SPIRAM);
    _z = (float*)heap_caps_malloc(vecSize, MALLOC_CAP_SPIRAM);
    _x = (float*)heap_caps_malloc(vecSize, MALLOC_CAP_SPIRAM);
#else
    _A = new float[MAX_DIM * MAX_DIM];
    _z = new float[MAX_DIM];
    _x = new float[MAX_DIM];
#endif

    if (!_A || !_z || !_x) {
        deallocate();
        return false;
    }

    stampClear();
    return true;
}

void MnaMatrix::deallocate() {
#ifdef ARDUINO
    if (_A) { heap_caps_free(_A); _A = nullptr; }
    if (_z) { heap_caps_free(_z); _z = nullptr; }
    if (_x) { heap_caps_free(_x); _x = nullptr; }
#else
    delete[] _A; _A = nullptr;
    delete[] _z; _z = nullptr;
    delete[] _x; _x = nullptr;
#endif
}

// ══ Union-Find ══════════════════════════════════════════════════════════════

void MnaMatrix::ufReset() {
    for (int i = 0; i < MAX_NODES; ++i)
        _ufParent[i] = i;
}

int MnaMatrix::ufFind(int n) {
    if (n < 0 || n >= MAX_NODES) return 0;
    while (_ufParent[n] != n) {
        _ufParent[n] = _ufParent[_ufParent[n]];  // path compression
        n = _ufParent[n];
    }
    return n;
}

void MnaMatrix::ufUnion(int a, int b) {
    int ra = ufFind(a);
    int rb = ufFind(b);
    if (ra != rb) {
        // Always prefer lower-numbered node as root (keep node 0 as GND)
        if (ra < rb)
            _ufParent[rb] = ra;
        else
            _ufParent[ra] = rb;
    }
}

// ══ Node-to-row mapping ════════════════════════════════════════════════════

int MnaMatrix::nodeToRow(int node) const {
    // Node 0 = ground (not in matrix).  Node n maps to row (n-1).
    if (node <= 0) return -1;  // ground
    if (node >= MAX_NODES) return -1;
    return node - 1;
}

// ══ Stamping ════════════════════════════════════════════════════════════════

void MnaMatrix::stampClear() {
    if (!_A || !_z || !_x) return;

    _dim = (MAX_NODES - 1) + _vsCount;
    if (_dim > MAX_DIM) _dim = MAX_DIM;

    memset(_A, 0, MAX_DIM * MAX_DIM * sizeof(float));
    memset(_z, 0, MAX_DIM * sizeof(float));
    memset(_x, 0, MAX_DIM * sizeof(float));

    // "Crash Saver": add parasitic conductance from every node to ground.
    // This prevents singular matrices from floating (unconnected) nodes.
    for (int n = 1; n < MAX_NODES; ++n) {
        int r = nodeToRow(n);
        if (r >= 0 && r < MAX_DIM)
            A(r, r) += PARASITIC_G;
    }
}

void MnaMatrix::stampConductance(int nodeA, int nodeB, float g) {
    if (!_A) return;

    // Apply Union-Find mapping
    nodeA = ufFind(nodeA);
    nodeB = ufFind(nodeB);

    int ra = nodeToRow(nodeA);
    int rb = nodeToRow(nodeB);

    // G_aa += g
    if (ra >= 0 && ra < MAX_DIM)
        A(ra, ra) += g;

    // G_bb += g
    if (rb >= 0 && rb < MAX_DIM)
        A(rb, rb) += g;

    // G_ab -= g, G_ba -= g
    if (ra >= 0 && rb >= 0 && ra < MAX_DIM && rb < MAX_DIM) {
        A(ra, rb) -= g;
        A(rb, ra) -= g;
    }
}

void MnaMatrix::stampResistor(int nodeA, int nodeB, float resistance) {
    if (resistance <= 0.0f) return;
    stampConductance(nodeA, nodeB, 1.0f / resistance);
}

void MnaMatrix::stampVoltageSource(int nodeP, int nodeN, float volts, int vsIdx) {
    if (!_A || !_z) return;
    if (vsIdx < 0 || vsIdx >= MAX_VSOURCES) return;

    // Apply Union-Find mapping
    nodeP = ufFind(nodeP);
    nodeN = ufFind(nodeN);

    int rp = nodeToRow(nodeP);
    int rn = nodeToRow(nodeN);

    // Voltage source current variable is at row: (MAX_NODES-1) + vsIdx
    int vsRow = (MAX_NODES - 1) + vsIdx;
    if (vsRow >= MAX_DIM) return;

    // B matrix: A[nodeP_row][vsRow] = +1, A[nodeN_row][vsRow] = -1
    if (rp >= 0 && rp < MAX_DIM) A(rp, vsRow) += 1.0f;
    if (rn >= 0 && rn < MAX_DIM) A(rn, vsRow) -= 1.0f;

    // C matrix (transpose of B): A[vsRow][nodeP_row] = +1, A[vsRow][nodeN_row] = -1
    if (rp >= 0 && rp < MAX_DIM) A(vsRow, rp) += 1.0f;
    if (rn >= 0 && rn < MAX_DIM) A(vsRow, rn) -= 1.0f;

    // z vector: voltage value
    _z[vsRow] = volts;
}

void MnaMatrix::stampCurrentSource(int nodeA, int nodeB, float amps) {
    if (!_z) return;

    nodeA = ufFind(nodeA);
    nodeB = ufFind(nodeB);

    int ra = nodeToRow(nodeA);
    int rb = nodeToRow(nodeB);

    // Current entering nodeA, leaving nodeB
    if (ra >= 0 && ra < MAX_DIM) _z[ra] += amps;
    if (rb >= 0 && rb < MAX_DIM) _z[rb] -= amps;
}

void MnaMatrix::stampCapacitor(int nodeA, int nodeB, float capacitance, float vPrev) {
    if (_dt <= 0.0f || capacitance <= 0.0f) return;

    // Backward Euler companion model:
    //   G_eq = C / dt
    //   I_eq = G_eq * V_prev = (C / dt) * V_prev
    float gEq = capacitance / _dt;
    stampConductance(nodeA, nodeB, gEq);
    stampCurrentSource(nodeA, nodeB, gEq * vPrev);
}

// ══ Solver: Gaussian Elimination with Partial Pivoting ══════════════════════

bool MnaMatrix::solve() {
    if (!_A || !_z || !_x) return false;

    _dim = (MAX_NODES - 1) + _vsCount;
    if (_dim > MAX_DIM) _dim = MAX_DIM;
    if (_dim <= 0) return false;

    // Copy z into x (we'll transform x in-place as augmented column)
    for (int i = 0; i < _dim; ++i)
        _x[i] = _z[i];

    // Forward elimination
    for (int col = 0; col < _dim; ++col) {
        // Partial pivoting: find the row with the largest absolute value
        int pivotRow = col;
        float pivotVal = fabsf(A(col, col));
        for (int row = col + 1; row < _dim; ++row) {
            float val = fabsf(A(row, col));
            if (val > pivotVal) {
                pivotVal = val;
                pivotRow = row;
            }
        }

        // Swap rows if needed
        if (pivotRow != col) {
            for (int j = 0; j < _dim; ++j) {
                float tmp = A(col, j);
                A(col, j) = A(pivotRow, j);
                A(pivotRow, j) = tmp;
            }
            float tmp = _x[col];
            _x[col] = _x[pivotRow];
            _x[pivotRow] = tmp;
        }

        // Check for near-singular
        if (fabsf(A(col, col)) < 1e-15f)
            return false;

        // Eliminate below
        for (int row = col + 1; row < _dim; ++row) {
            float factor = A(row, col) / A(col, col);
            for (int j = col; j < _dim; ++j)
                A(row, j) -= factor * A(col, j);
            _x[row] -= factor * _x[col];
        }
    }

    // Back substitution
    for (int row = _dim - 1; row >= 0; --row) {
        float sum = _x[row];
        for (int j = row + 1; j < _dim; ++j)
            sum -= A(row, j) * _x[j];
        _x[row] = sum / A(row, row);
    }

    return true;
}

// ══ Result accessors ════════════════════════════════════════════════════════

float MnaMatrix::nodeVoltage(int node) const {
    if (!_x) return 0.0f;
    // Perform Union-Find lookup without path compression for const safety
    int n = node;
    if (n < 0 || n >= MAX_NODES) return 0.0f;
    while (_ufParent[n] != n) n = _ufParent[n];
    if (n <= 0) return 0.0f;  // ground
    int r = nodeToRow(n);
    if (r < 0 || r >= _dim) return 0.0f;
    return _x[r];
}

float MnaMatrix::vsCurrent(int vsIdx) const {
    if (!_x) return 0.0f;
    if (vsIdx < 0 || vsIdx >= _vsCount) return 0.0f;
    int r = (MAX_NODES - 1) + vsIdx;
    if (r >= _dim) return 0.0f;
    return _x[r];
}
