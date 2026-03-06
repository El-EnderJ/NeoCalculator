/**
 * MatrixEngine.h — Matrix algebra engine for NumOS.
 *
 * Static-allocation matrix operations for small matrices (up to 5×5):
 *   - Add, Subtract, Multiply
 *   - Determinant (cofactor expansion)
 *   - Inverse (Gauss-Jordan elimination)
 *
 * All operations guard against division by zero and dimension mismatches.
 *
 * Part of: NumOS — Linear Algebra Suite
 */

#pragma once

#include <cstdint>
#include <cstddef>

class MatrixEngine {
public:
    // ── Constants ────────────────────────────────────────────────────────
    static constexpr int MAX_DIM = 5;

    // ── Matrix data structure (static allocation) ────────────────────────
    struct Matrix {
        double data[MAX_DIM][MAX_DIM];
        int rows;
        int cols;

        Matrix() : rows(0), cols(0) {
            for (int r = 0; r < MAX_DIM; ++r)
                for (int c = 0; c < MAX_DIM; ++c)
                    data[r][c] = 0.0;
        }

        Matrix(int r, int c) : rows(r), cols(c) {
            for (int i = 0; i < MAX_DIM; ++i)
                for (int j = 0; j < MAX_DIM; ++j)
                    data[i][j] = 0.0;
        }

        double& at(int r, int c) { return data[r][c]; }
        double  at(int r, int c) const { return data[r][c]; }
    };

    // ── Result wrapper ───────────────────────────────────────────────────
    struct Result {
        bool   ok;
        Matrix mat;
        double scalar;       // Used for determinant
        const char* error;   // Error message (nullptr if ok)

        Result() : ok(false), scalar(0), error(nullptr) {}
    };

    // ── Operations ───────────────────────────────────────────────────────

    /** C = A + B (dimensions must match) */
    static Result add(const Matrix& A, const Matrix& B);

    /** C = A - B (dimensions must match) */
    static Result subtract(const Matrix& A, const Matrix& B);

    /** C = A × B (A.cols must equal B.rows) */
    static Result multiply(const Matrix& A, const Matrix& B);

    /** det(A) — square matrix only */
    static Result determinant(const Matrix& A);

    /** A^-1 via Gauss-Jordan — square matrix, non-singular */
    static Result inverse(const Matrix& A);

private:
    /**
     * Recursive determinant via cofactor expansion.
     * @param m   Matrix data (flat view)
     * @param n   Current sub-matrix size
     */
    static double det(double m[MAX_DIM][MAX_DIM], int n);

    /** Extract minor (remove row r, col c) */
    static void getMinor(const double src[MAX_DIM][MAX_DIM], double dst[MAX_DIM][MAX_DIM],
                         int r, int c, int n);
};
