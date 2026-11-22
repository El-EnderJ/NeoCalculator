#pragma once
// Owned, bounded quantity values; no Giac, Toolbox, LVGL or context in the model.
#include "MathAST.h"
#include "units/UnitRegistry.h"
#include "ScientificUncertainty.h"
#include <array>
#include <memory>
#include <string>

namespace numos::quantity {
constexpr unsigned kDimensions=17; // seven SI, plane/solid angle, information, seven counts
constexpr unsigned kComponents=17;
constexpr int kExponentLimit=64;
struct Dimension {
    std::array<int8_t,kDimensions> powers{};
    bool operator==(const Dimension& b)const{return powers==b.powers;}
    bool operator!=(const Dimension& b)const{return !(*this==b);}
    bool scalar()const {for(auto p:powers)if(p)return false;return true;}
};
enum class Error:uint8_t {None,Incomplete,DimensionMismatch,DivisionByZero,AffinePending,
    ContextPending,UncertaintyPending,ApproximatePending,SymbolicPending,ComplexPending,
    FunctionUnsupported,ExponentLimit,ExpressionLimit,InvalidIdentity,Allocation,ScalarDomain,ReferencePending,
    AffineOperation,UncertaintyInvalid,UncertaintyLimit,UncertaintyNumeric,
    CorrelationUnavailable,UncertaintyNonlinear,SourceExhausted};
const char* errorName(Error);
const char* errorMessage(Error,bool spanish);
struct Value {
    std::string coefficient; // exact scalar in the coherent reference, owned
    Dimension dimension;
    // WHY: an immutable mathematical role; display conversions cannot change it.
    enum class Role:uint8_t { Linear, TemperaturePoint, TemperatureDifference };
    Role role=Role::Linear;
    units::UnitId meaning{}; // provenance only; zero means unspecified magnitude
    enum class Provenance:uint8_t { Exact, Measured, DerivedUncertain };
    Provenance provenance=Provenance::Exact;
    // Canonical trigonometric functions are radians, independent of input mode.
    // Retain even a dimensionless exact Quantity while such functions remain.
    bool radianFunctions=false;
    // Shared immutable sensitivities retain identity through Ans/history/A-F.
    // nullptr costs no payload allocation, including exact source cancellation.
    scientific::Owned uncertainty;
};
using Owned=std::shared_ptr<const Value>;
struct Descriptor {
    std::array<units::Term,kComponents> terms{};
    uint8_t count=0;
    uint32_t numerator=1,denominator=1;
};
struct Display {
    std::string exact,approximate;
    vpam::NodePtr exactCoefficient,approximateCoefficient;
    double nominal=0,standardUncertainty=0,relativeUncertainty=0;
    bool hasUncertainty=false,relativeDefined=false;
};
using Resolve=bool(*)(const void*,char,Value&);
struct Analysis {Value value;Error error=Error::None;unsigned calls=0;};
Error admissible(units::Atom);
Dimension dimension(units::Atom);
Analysis evaluate(const vpam::MathNode*,const void* owner,Resolve);
bool contains(const vpam::MathNode*,const void* owner,Resolve);
bool descriptor(uint16_t item,uint8_t prefix,Descriptor&);
bool descriptorDimension(const Descriptor&,Dimension&);
bool compatible(const Value&,const Descriptor&);
Descriptor coherent(const Value&);
Error convert(const Value&,const Descriptor&,Display&);
Error standardUncertainty(const Value&,double&);
Error measurement(Value nominal,const Value& uncertainty,uint32_t sourceId,Value&);
vpam::NodePtr compose(vpam::NodePtr coefficient,const Descriptor&);
bool symbol(const Descriptor&,char*,size_t);
}
