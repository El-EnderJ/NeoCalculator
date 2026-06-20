/*
 * NeoCalculator - NumOS
 * Copyright (C) 2026 Juan Ramon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/**
 * CASStepLogger.cpp — Implementation of step-by-step CAS logger.
 *
 * Includes hash-based step deduplication: each step is fingerprinted
 * using the equation snapshot string and expression pointer hash.
 * If current_hash == previous_hash the step is silently discarded,
 * eliminating the redundant-step bug in solver output.
 *
 * Part of: NumOS CAS-S3 — Phase C+
 */

#include "CASStepLogger.h"
#include <cstdint>      // uintptr_t
#include <functional>   // std::hash

namespace cas {

// Fold 64-bit mixer constants down to the native word size so the logger keeps
// good entropy on 32-bit ESP targets without compile-time overflow warnings.
//
// On 32-bit size_t (ESP32 firmware) this folds the high half into the low half:
// value ^ (value >> 32) — the original, unchanged behaviour. On 64-bit size_t
// (native PC emulator) size_t already holds the whole 64-bit value, and shifting
// a 64-bit value by 64 is undefined behaviour, so we return it unfolded. The
// `& 63U` keeps the shift count in range on both targets (no -Wshift-count-overflow).
static constexpr size_t foldHashConstant(uint64_t value) {
    return (sizeof(size_t) < sizeof(uint64_t))
        ? static_cast<size_t>(value ^ (value >> ((sizeof(size_t) * 8U) & 63U)))
        : static_cast<size_t>(value);
}

// Hash mixing constants (SplitMix64 / FNV-derived)
static constexpr size_t EXPR_HASH_MULTIPLIER = foldHashConstant(UINT64_C(0x9e3779b97f4a7c15));
static constexpr size_t KIND_HASH_MULTIPLIER = foldHashConstant(UINT64_C(0x517cc1b727220a95));

// ────────────────────────────────────────────────────────────────────
// Constructor
// ────────────────────────────────────────────────────────────────────

CASStepLogger::CASStepLogger()
    : _steps(), _prevHash(0) {}

// ────────────────────────────────────────────────────────────────────
// computeStepHash — fingerprint a step's mathematical content
//
// Combines the serialised equation snapshot with the SymExpr pointer
// hash (if available) and the step kind.  This produces a lightweight
// fingerprint suitable for consecutive-duplicate detection.
// ────────────────────────────────────────────────────────────────────

size_t CASStepLogger::computeStepHash(const SymEquation& snapshot,
                                       const SymExpr* expr,
                                       StepKind kind) {
    std::hash<std::string> strHash;
    size_t h = strHash(snapshot.toString());
    // Mix in the expression pointer hash when available (hash-consed
    // pointers are unique per structure, so this is meaningful).
    if (expr) {
        h ^= static_cast<size_t>(reinterpret_cast<uintptr_t>(expr))
             * EXPR_HASH_MULTIPLIER;
    }
    // Mix in kind so that a Transform and a Result with the same
    // snapshot are not considered duplicates.
    h ^= static_cast<size_t>(kind) * KIND_HASH_MULTIPLIER;
    return h;
}

// ────────────────────────────────────────────────────────────────────
// isDuplicate — returns true when the step should be discarded
// ────────────────────────────────────────────────────────────────────

bool CASStepLogger::isDuplicate(size_t hash, StepKind kind) const {
    // Annotations are never deduplicated — they carry unique text.
    if (kind == StepKind::Annotation) return false;

    // First step is never a duplicate.
    if (_steps.empty()) return false;

    return hash == _prevHash;
}

// ────────────────────────────────────────────────────────────────────
// log — Record a transform step with equation snapshot
// ────────────────────────────────────────────────────────────────────

void CASStepLogger::log(const std::string& description,
                        const SymEquation& snapshot,
                        MethodId method,
                        const std::string& reason) {
    size_t h = computeStepHash(snapshot, nullptr, StepKind::Transform);
    if (isDuplicate(h, StepKind::Transform)) return;   // Dedup!
    _prevHash = h;
    _steps.emplace_back(description, snapshot, method, StepKind::Transform,
                        nullptr, reason);
}

// ────────────────────────────────────────────────────────────────────
// logNote — Record an annotation step (text-only, no equation)
// ────────────────────────────────────────────────────────────────────

void CASStepLogger::logNote(const std::string& note, MethodId method) {
    _steps.emplace_back(note, SymEquation(), method, StepKind::Annotation);
}

// ────────────────────────────────────────────────────────────────────
// logExpr — Record an annotation step with MathCanvas-ready expression
// ────────────────────────────────────────────────────────────────────

void CASStepLogger::logExpr(const std::string& desc, const SymExpr* expr,
                            MethodId method,
                            const std::string& reason) {
    // Annotation steps are not deduplicated — each carries unique text.
    // Update _prevHash so subsequent Transform/Result steps can dedup
    // against the mathematical state represented by this expression.
    size_t h = computeStepHash(SymEquation(), expr, StepKind::Annotation);
    _prevHash = h;
    _steps.emplace_back(desc, SymEquation(), method, StepKind::Annotation,
                        expr, reason);
}

// ────────────────────────────────────────────────────────────────────
// logWithHighlight — Record a step with a highlighted sub-expression
//
// Identical to logExpr but stores an additional highlightExpr pointer
// that the step viewer can use to render the modified sub-expression
// in an accent colour (Smart Highlighter visual cue).
// ────────────────────────────────────────────────────────────────────

void CASStepLogger::logWithHighlight(const std::string& desc,
                                     const SymExpr* expr,
                                     const SymExpr* highlightExpr,
                                     MethodId method,
                                     const std::string& reason) {
    size_t h = computeStepHash(SymEquation(), expr, StepKind::Annotation);
    _prevHash = h;
    _steps.emplace_back(desc, SymEquation(), method, StepKind::Annotation,
                        expr, reason, highlightExpr);
}

// ────────────────────────────────────────────────────────────────────
// logResult — Record a final result step with equation snapshot
// ────────────────────────────────────────────────────────────────────

void CASStepLogger::logResult(const std::string& description,
                              const SymEquation& snapshot,
                              MethodId method,
                              const std::string& reason) {
    size_t h = computeStepHash(snapshot, nullptr, StepKind::Result);
    if (isDuplicate(h, StepKind::Result)) return;
    _prevHash = h;
    _steps.emplace_back(description, snapshot, method, StepKind::Result,
                        nullptr, reason);
}

// ────────────────────────────────────────────────────────────────────
// logResultExpr — Record a final result step with MathCanvas expression
// ────────────────────────────────────────────────────────────────────

void CASStepLogger::logResultExpr(const std::string& desc,
                                  const SymExpr* expr,
                                  MethodId method,
                                  const std::string& reason) {
    size_t h = computeStepHash(SymEquation(), expr, StepKind::Result);
    if (isDuplicate(h, StepKind::Result)) return;
    _prevHash = h;
    _steps.emplace_back(desc, SymEquation(), method, StepKind::Result,
                        expr, reason);
}

// ────────────────────────────────────────────────────────────────────
// logComplex — Record a complex root result (text-only)
// ────────────────────────────────────────────────────────────────────

void CASStepLogger::logComplex(const std::string& description,
                               MethodId method) {
    _steps.emplace_back(description, SymEquation(), method, StepKind::ComplexResult);
}

// ────────────────────────────────────────────────────────────────────
// copyStep — Copy a step preserving its original StepKind
// ────────────────────────────────────────────────────────────────────

void CASStepLogger::copyStep(const CASStep& step) {
    size_t h = computeStepHash(step.snapshot, step.mathExpr, step.kind);
    if (isDuplicate(h, step.kind)) return;
    _prevHash = h;
    _steps.emplace_back(step.description, step.snapshot, step.method, step.kind,
                        step.mathExpr, step.reason, step.highlightExpr);
}

// ────────────────────────────────────────────────────────────────────
// clear
// ────────────────────────────────────────────────────────────────────

void CASStepLogger::clear() {
    _steps.clear();
    _prevHash = 0;
}

// ────────────────────────────────────────────────────────────────────
// dump — Returns all steps as multi-line string
// ────────────────────────────────────────────────────────────────────

std::string CASStepLogger::dump() const {
    std::string out;
    int i = 1;
    for (const auto& step : _steps) {
        out += "Step " + std::to_string(i++) + ": " + step.description + "\n";
        if (!step.reason.empty()) {
            out += "  [" + step.reason + "]\n";
        }
        // Only show equation for Transform and Result steps
        if (step.kind == StepKind::Transform || step.kind == StepKind::Result) {
            std::string eqStr = step.snapshot.toString();
            if (!eqStr.empty() && eqStr != "0 = 0") {
                out += "  => " + eqStr + "\n";
            }
        }
    }
    return out;
}

} // namespace cas
