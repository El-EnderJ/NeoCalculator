#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#ifndef NUMOS_PRODUCTION_DEMO_PROFILE
#define NUMOS_PRODUCTION_DEMO_PROFILE 0
#endif

namespace numos::demo {

struct AppPolicy {
    int id;
    const char* name;
    const char* inclusionReason;
    bool safeModeVisible;
};

// WHY: IDs are the established launcher contract. Filtering never renumbers
// cards, so normal firmware, scripted input and persisted references cannot
// drift when the demo capability is enabled.
inline constexpr std::array<AppPolicy, 5> kEventReadyApps{{
    {0, "Calculation",
     "Core event workflow; Giac-authoritative structured calculation.", true},
    {1, "Grapher",
     "Core event workflow; parse-once retained Giac graph evaluation.", false},
    {2, "Equations",
     "Bounded equation/system editor using the centralized Giac seam.", false},
    {3, "Calculus",
     "Bounded derivative/integral surface with Giac as answer authority.", false},
    {10, "Settings",
     "Reduced offline settings and display/recovery entry surface.", true},
}};

constexpr const AppPolicy* findAppPolicy(const int id) {
    for (const auto& policy : kEventReadyApps) {
        if (policy.id == id) return &policy;
    }
    return nullptr;
}

constexpr bool isEventReadyApp(const int id) {
    return findAppPolicy(id) != nullptr;
}

constexpr bool isSafeModeApp(const int id) {
    const AppPolicy* policy = findAppPolicy(id);
    return policy && policy->safeModeVisible;
}

static_assert(kEventReadyApps.front().id == 0,
              "Calculation must remain the deterministic first launcher card");
static_assert(kEventReadyApps[1].id == 1,
              "Grapher must remain the second demo launcher card");

} // namespace numos::demo
