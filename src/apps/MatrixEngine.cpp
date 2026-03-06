/**
 * MatrixEngine.cpp — Matrix algebra engine.
 *
 * Implements: Add, Subtract, Multiply, Determinant, Inverse.
 * All operations use static allocation (no dynamic memory).
 * Division-by-zero guards are included for determinant/inverse.
 */

#include "MatrixEngine.h"
#include <cmath>
#include <cstring>

// ════════════════════════════════════════════════════════════════════════════
// Add — C = A + B
// ════════════════════════════════════════════════════════════════════════════

MatrixEngine::Result MatrixEngine::add(const Matrix& A, const Matrix& B) {
    Result res;
    if (A.rows != B.rows || A.cols != B.cols) {
        res.error = "Dimension Error";
        return res;
    }
    res.ok = true;
    res.mat = Matrix(A.rows, A.cols);
    for (int r = 0; r < A.rows; ++r)
        for (int c = 0; c < A.cols; ++c)
            res.mat.data[r][c] = A.data[r][c] + B.data[r][c];
    return res;
}

// ════════════════════════════════════════════════════════════════════════════
// Subtract — C = A - B
// ════════════════════════════════════════════════════════════════════════════

MatrixEngine::Result MatrixEngine::subtract(const Matrix& A, const Matrix& B) {
    Result res;
    if (A.rows != B.rows || A.cols != B.cols) {
        res.error = "Dimension Error";
        return res;
    }
    res.ok = true;
    res.mat = Matrix(A.rows, A.cols);
    for (int r = 0; r < A.rows; ++r)
        for (int c = 0; c < A.cols; ++c)
            res.mat.data[r][c] = A.data[r][c] - B.data[r][c];
    return res;
}

// ════════════════════════════════════════════════════════════════════════════
// Multiply — C = A × B
// ════════════════════════════════════════════════════════════════════════════

MatrixEngine::Result MatrixEngine::multiply(const Matrix& A, const Matrix& B) {
    Result res;
    if (A.cols != B.rows) {
        res.error = "Dimension Error";
        return res;
    }
    if (A.rows > MAX_DIM || B.cols > MAX_DIM) {
        res.error = "Matrix Too Large";
        return res;
    }
    res.ok = true;
    res.mat = Matrix(A.rows, B.cols);
    for (int r = 0; r < A.rows; ++r)
        for (int c = 0; c < B.cols; ++c) {
            double sum = 0;
            for (int k = 0; k < A.cols; ++k)
                sum += A.data[r][k] * B.data[k][c];
            res.mat.data[r][c] = sum;
        }
    return res;
}

// ════════════════════════════════════════════════════════════════════════════
// Determinant — cofactor expansion
// ════════════════════════════════════════════════════════════════════════════

MatrixEngine::Result MatrixEngine::determinant(const Matrix& A) {
    Result res;
    if (A.rows != A.cols) {
        res.error = "Not Square";
        return res;
    }
    if (A.rows < 1 || A.rows > MAX_DIM) {
        res.error = "Invalid Size";
        return res;
    }

    // Copy to mutable temp
    double temp[MAX_DIM][MAX_DIM];
    for (int r = 0; r < A.rows; ++r)
        for (int c = 0; c < A.cols; ++c)
            temp[r][c] = A.data[r][c];

    res.ok = true;
    res.scalar = det(temp, A.rows);
    return res;
}

double MatrixEngine::det(double m[MAX_DIM][MAX_DIM], int n) {
    if (n == 1) return m[0][0];
    if (n == 2) return m[0][0] * m[1][1] - m[0][1] * m[1][0];

    double result = 0;
    double minor[MAX_DIM][MAX_DIM];
    double sign = 1.0;

    for (int c = 0; c < n; ++c) {
        getMinor(m, minor, 0, c, n);
        result += sign * m[0][c] * det(minor, n - 1);
        sign = -sign;
    }
    return result;
}

void MatrixEngine::getMinor(const double src[MAX_DIM][MAX_DIM],
                             double dst[MAX_DIM][MAX_DIM],
                             int row, int col, int n) {
    int dr = 0;
    for (int r = 0; r < n; ++r) {
        if (r == row) continue;
        int dc = 0;
        for (int c = 0; c < n; ++c) {
            if (c == col) continue;
            dst[dr][dc] = src[r][c];
            ++dc;
        }
        ++dr;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Inverse — Gauss-Jordan elimination
// ════════════════════════════════════════════════════════════════════════════

MatrixEngine::Result MatrixEngine::inverse(const Matrix& A) {
    Result res;
    if (A.rows != A.cols) {
        res.error = "Not Square";
        return res;
    }
    int n = A.rows;
    if (n < 1 || n > MAX_DIM) {
        res.error = "Invalid Size";
        return res;
    }

    // Build augmented matrix [A | I]
    double aug[MAX_DIM][MAX_DIM * 2];
    memset(aug, 0, sizeof(aug));

    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c)
            aug[r][c] = A.data[r][c];
        aug[r][n + r] = 1.0;  // Identity
    }

    // Forward elimination with partial pivoting
    for (int col = 0; col < n; ++col) {
        // Find pivot row (largest absolute value in column)
        int pivotRow = col;
        double maxVal = std::fabs(aug[col][col]);
        for (int r = col + 1; r < n; ++r) {
            double v = std::fabs(aug[r][col]);
            if (v > maxVal) {
                maxVal = v;
                pivotRow = r;
            }
        }

        // Check for singular matrix
        if (maxVal < 1e-12) {
            res.error = "Singular Matrix";
            return res;
        }

        // Swap rows if needed
        if (pivotRow != col) {
            for (int c = 0; c < 2 * n; ++c) {
                double tmp = aug[col][c];
                aug[col][c] = aug[pivotRow][c];
                aug[pivotRow][c] = tmp;
            }
        }

        // Scale pivot row
        double pivot = aug[col][col];
        for (int c = 0; c < 2 * n; ++c)
            aug[col][c] /= pivot;

        // Eliminate column in all other rows
        for (int r = 0; r < n; ++r) {
            if (r == col) continue;
            double factor = aug[r][col];
            for (int c = 0; c < 2 * n; ++c)
                aug[r][c] -= factor * aug[col][c];
        }
    }

    // Extract inverse from augmented matrix
    res.ok = true;
    res.mat = Matrix(n, n);
    for (int r = 0; r < n; ++r)
        for (int c = 0; c < n; ++c)
            res.mat.data[r][c] = aug[r][n + c];

    return res;
}
