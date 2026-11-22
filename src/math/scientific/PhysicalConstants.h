#pragma once
// Dataset values are data, never UI positions or mathematical identity.
#include <cstddef>
#include <cstdint>

namespace numos::scientific {
enum class ConstantId : uint16_t {
    Invalid=0, SpeedOfLight=1, BohrRadius=2, ElectronMass=3,
    HartreeEnergy=4, AtomicTime=5, ReducedPlanck=6, PlanckLength=7,
    PlanckTime=8, PlanckMass=9, PlanckForce=10, PlanckEnergy=11,
    PlanckTemperature=12, StandardGravity=13, ElementaryCharge=14,
    Gravitational=15
};
enum class ConstantDatasetVersion : uint16_t { CODATA2022=1 };
inline constexpr ConstantDatasetVersion kCurrentDataset=ConstantDatasetVersion::CODATA2022;
inline constexpr const char* kDatasetName="CODATA 2022";
inline constexpr const char* kNistWebVersion="9.0";
struct ConstantRecord {
    ConstantId id;
    uint16_t referenceId;
    const char* nominal; // Exact mathematical expression for recommended central value.
    int8_t dimension[7]; // Registry order: L, M, T, I, temperature, amount, luminosity.
    double standardUncertainty;
    bool exact; // Physical exactness, independent of exact decimal storage.
    const char *symbol, *unit, *uncertaintyText;
};
// WHY: future dataset changes preserve ConstantId and existing session sources.
constexpr uint32_t datasetSource(ConstantId id) noexcept {
    return UINT32_C(0x80000000)|(uint32_t(kCurrentDataset)<<16)|uint16_t(id);
}
const ConstantRecord* constant(ConstantId) noexcept;
const ConstantRecord* byReference(uint16_t) noexcept;
size_t constantCount() noexcept;
// false means unavailable, including unsupported IDs; it never means rho=0.
bool correlation(ConstantId,ConstantId,double& rho) noexcept;
}
