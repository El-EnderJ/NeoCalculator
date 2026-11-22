#pragma once
#include "Quantity.h"
#include "CalculationFormat.h"
#include <cmath>
#include <cstdio>

namespace numos::scientific {
// Reporting is prepared outside layout/paint and never feeds back to Quantity.
// JCGM 100:2008 §7.2.6: normally at most two significant digits in u, with the
// estimate rounded to the corresponding place. Decimal retains more digits.
inline vpam::NodePtr reportingNumber(const std::string& text) {
    DecimalParts parts;
    if(!decimalParts(text,parts))return {};
    if(text.find_first_of("eE")!=std::string::npos)return powerOfTenFormat(text,false);
    auto row=vpam::makeRow(); auto* r=static_cast<vpam::NodeRow*>(row.get());
    if(!text.empty() && text[0]=='-') {r->appendChild(vpam::makeOperator(vpam::OpKind::Sub));r->appendChild(vpam::makeNumber(text.substr(1)));}
    else r->appendChild(vpam::makeNumber(text));
    return row;
}
inline std::string reportingDigits(double value) {
    char text[48]{};std::snprintf(text,sizeof(text),"%.17g",value);return text;
}
inline vpam::NodePtr uncertaintyReport(const quantity::Display& view,
        const quantity::Descriptor& unit, CalculationFormat mode) {
    using F=CalculationFormat;
    if(!view.hasUncertainty || !std::isfinite(view.nominal) || !std::isfinite(view.standardUncertainty))return {};
    auto row=vpam::makeRow();auto* r=static_cast<vpam::NodeRow*>(row.get());
    bool complete=true;
    auto append=[&](vpam::NodePtr n){if(n)r->appendChild(std::move(n));else complete=false;};
    const bool point=unit.count==1 && units::normalized(unit.terms[0].atom).role==units::TemperatureRole::Absolute;
    if(mode==F::UncertaintyRelative) {
        append(quantity::compose(vpam::cloneNode(view.approximateCoefficient?view.approximateCoefficient.get():view.exactCoefficient.get()),unit));
        append(vpam::makeSymbol(";"));
        append(vpam::makeSubscript(vpam::makeSymbol("u"),vpam::makeSymbol(point?"r(K)":"r")));
        append(vpam::makeSymbol("="));
        if(view.relativeDefined){
            const bool percent=view.relativeUncertainty<=std::numeric_limits<double>::max()/100;
            char relative[48]{};std::snprintf(relative,sizeof(relative),"%.6g",view.relativeUncertainty*(percent?100:1));
            append(reportingNumber(relative));
            if(percent)append(vpam::makeSymbol("%"));
        }
        else append(vpam::makeSymbol("—"));
        return complete?std::move(row):vpam::NodePtr{};
    }
    const bool expanded=mode==F::UncertaintyExpanded;
    const bool parenthetic=mode==F::UncertaintyParenthetical;
    const double u=view.standardUncertainty*(expanded?2:1);
    if(!std::isfinite(u))return {};
    std::string nominalText=reportingDigits(view.nominal),uText=reportingDigits(u),parentDigits;
    int exponent=0;
    if(mode!=F::Decimal && u>0) {
        int uExponent=static_cast<int>(std::floor(std::log10(u)));
        const double quantum=std::pow(10.0,uExponent-1);
        // WHY: 0.999 rounds to 1.0, not 1.00; carry changes the last place.
        if(quantum>0 && std::isfinite(quantum) && std::round(u/quantum)>=100)++uExponent;
        const int yExponent=view.nominal==0?uExponent:static_cast<int>(std::floor(std::log10(std::abs(view.nominal))));
        const int largest=std::max(yExponent,uExponent);
        exponent=(largest>=5 || largest<-3 || uExponent>1)?largest:0;
        const int places=1-uExponent+exponent;
        if(places>=0 && places<=12 && exponent>=-300 && exponent<=300) {
            const double scale=std::pow(10.0,exponent);
            char y[96]{},s[96]{},p[32]{};
            std::snprintf(y,sizeof(y),"%.*f",places,view.nominal/scale);
            std::snprintf(s,sizeof(s),"%.*f",places,u/scale);
            std::snprintf(p,sizeof(p),"%.0f",(u/scale)*std::pow(10.0,places));
            nominalText=y;uText=s;parentDigits=p;
            if(!nominalText.empty() && nominalText[0]=='-' && std::strtod(y,nullptr)==0)nominalText.erase(0,1);
        } else exponent=0; // Extreme scale separation: show both independently.
    }
    if(parenthetic && !parentDigits.empty()) {
        auto coefficient=vpam::makeRow();auto* c=static_cast<vpam::NodeRow*>(coefficient.get());
        c->appendChild(reportingNumber(nominalText));c->appendChild(vpam::makeParen(reportingNumber(parentDigits)));
        if(exponent){c->appendChild(vpam::makeOperator(vpam::OpKind::Mul));c->appendChild(vpam::makePower(vpam::makeNumber("10"),reportingNumber(std::to_string(exponent))));}
        append(quantity::compose(std::move(coefficient),unit));return complete?std::move(row):vpam::NodePtr{};
    }
    auto nominal=reportingNumber(nominalText);
    auto uncertainty=reportingNumber(uText);
    if(!nominal || !uncertainty)return {};
    if(point) {
        // WHY: the right-hand quantity is a temperature DIFFERENCE. Offsets
        // have already been excluded by convert(); show the distinction too.
        auto difference=unit;difference.terms[0].atom.role=units::TemperatureRole::Difference;
        if(exponent) {
            nominal=powerOfTenFormat(reportingDigits(view.nominal),false);
            uncertainty=powerOfTenFormat(reportingDigits(u),false);
        }
        append(vpam::makeMeasurement(quantity::compose(std::move(nominal),unit),quantity::compose(std::move(uncertainty),difference),UINT32_MAX));
    } else {
        auto coefficient=vpam::makeRow();auto* c=static_cast<vpam::NodeRow*>(coefficient.get());
        c->appendChild(vpam::makeMeasurement(std::move(nominal),std::move(uncertainty),UINT32_MAX));
        if(exponent){c->appendChild(vpam::makeOperator(vpam::OpKind::Mul));c->appendChild(vpam::makePower(vpam::makeNumber("10"),reportingNumber(std::to_string(exponent))));}
        append(quantity::compose(std::move(coefficient),unit));
    }
    if(expanded)append(vpam::makeSymbol(" k=2"));
    return complete?std::move(row):vpam::NodePtr{};
}
}
