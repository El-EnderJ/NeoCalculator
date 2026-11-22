#pragma once
// Bounded first-order sensitivities. No evaluator, AST, units or UI dependencies.
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

namespace numos::scientific {
using SourceId=uint32_t;
inline constexpr unsigned kMaximumSources=16;
inline constexpr SourceId kLastUserSource=UINT32_C(0x7fffffff);
inline constexpr SourceId kDisplaySource=UINT32_MAX;

// WHY: saturating CAS never wraps and never reuses an ID still retained by Ans,
// memory or a cloned history AST. A separate sequence permits a real wrap test.
class SourceSequence {
    std::atomic<SourceId> next_;
public:
    explicit constexpr SourceSequence(SourceId next=1) noexcept:next_(next){}
    SourceId allocate() noexcept {
        auto next=next_.load(std::memory_order_relaxed);
        while(next && next<=kLastUserSource) {
            if(next_.compare_exchange_weak(next,next+1,std::memory_order_relaxed))return next;
        }
        return 0;
    }
};
inline SourceSequence userSources;
inline SourceId allocateUserSource() noexcept {return userSources.allocate();}
inline constexpr bool isUserSource(SourceId id) noexcept {return id && id<=kLastUserSource;}

enum class UncertaintyError:uint8_t {
    None,Invalid,Limit,Numeric,CorrelationUnavailable,Nonlinear,SourceExhausted,Allocation
};
struct Contribution {SourceId source=0;double coefficient=0;};
struct Uncertainty {
    // WHY: capacity belongs only to uncertain payloads, never to every Quantity
    // or history entry. Sorted IDs allow bounded linear merges and cancellation.
    std::array<Contribution,kMaximumSources> contributions{};
    uint8_t count=0;
};
using Owned=std::shared_ptr<const Uncertainty>;
using CorrelationResolver=bool(*)(SourceId,SourceId,double&) noexcept;

bool sourceCorrelation(SourceId,SourceId,double&) noexcept;
UncertaintyError makeSource(SourceId,double standardUncertainty,Owned&);
UncertaintyError combine(const Owned&,double derivativeA,const Owned&,double derivativeB,Owned&);
UncertaintyError scale(const Owned&,double derivative,Owned&);
UncertaintyError standardDeviation(const Owned&,double&,CorrelationResolver=sourceCorrelation) noexcept;

// These operations do not decide mathematical equality, dimension or nominal
// exactness. x/y are numeric companions used exclusively for sensitivities.
UncertaintyError product(const Owned&,double x,const Owned&,double y,bool divide,Owned&);
UncertaintyError power(const Owned&,double x,int numerator,int denominator,Owned&);
enum class Unary:uint8_t {Abs,Exp,Ln,Log10,Sin,Cos,Tan};
UncertaintyError unary(const Owned&,double x,Unary,Owned&);
}
