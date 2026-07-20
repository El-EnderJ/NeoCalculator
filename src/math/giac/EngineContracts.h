/*
 * EngineContracts.h — engine-neutral limits shared by NumOS Giac adapters.
 *
 * This header deliberately contains no Giac types.  It centralizes only
 * contracts that are byte-for-byte identical across Calculation, Calculus,
 * Equations, and NeoMathBackend; app-specific policy stays with each caller.
 */
#pragma once

#include <cctype>
#include <cstddef>
#include <string_view>

namespace numos::enginecontract {

inline constexpr int kMaxTreeDepth = 40;
inline constexpr int kMaxTreeNodes = 400;
inline constexpr std::size_t kMaxSourceBytes = 2000;

// WHY: all public adapters accept the same ASCII identifier grammar.  Maximum
// length and reserved-name policy remain caller-owned because those contracts
// genuinely differ (Neo: 63; Giac solve/calculus: 31 plus different keywords).
inline bool isPlainIdentifier(
    std::string_view name,
    std::size_t maxLength = static_cast<std::size_t>(-1)) {
    if (name.empty() || name.size() > maxLength) return false;
    const unsigned char first =
        static_cast<unsigned char>(name.front());
    if (!(std::isalpha(first) || name.front() == '_')) return false;
    for (const char c : name) {
        const unsigned char value = static_cast<unsigned char>(c);
        if (!(std::isalnum(value) || c == '_')) return false;
    }
    return true;
}

} // namespace numos::enginecontract
