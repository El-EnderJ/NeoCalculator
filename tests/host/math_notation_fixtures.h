// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "math/CalculationEngine.h"
#include <vector>
namespace notationtest {
using numos::EngineResultNode;
using K = numos::EngineNodeKind;
inline EngineResultNode node(K k, const char* s = "", std::initializer_list<EngineResultNode> children = {}) {
    EngineResultNode n; n.kind = k; n.text = s; n.children = children; return n;
}
inline EngineResultNode integer(const char* s) { return node(K::Integer,s); }
inline EngineResultNode symbol(const char* s) { return node(K::Symbol,s); }
inline EngineResultNode mul(EngineResultNode a, EngineResultNode b) { return node(K::Mul,"",{a,b}); }
inline EngineResultNode add(EngineResultNode a, EngineResultNode b) { return node(K::Add,"",{a,b}); }
inline EngineResultNode square(EngineResultNode a) { return node(K::Pow,"",{a,integer("2")}); }
inline EngineResultNode fraction(const char* a, const char* b) { return node(K::Rational,"",{integer(a),integer(b)}); }
struct Case { const char* id; EngineResultNode tree; unsigned visible; };
inline std::vector<Case> cases() {
    auto x=symbol("x"),a=symbol("a"),b=symbol("b"),c=symbol("c");
    auto plus=add(x,integer("1")),minus=add(x,integer("-1"));
    auto matrix=node(K::Matrix,"",{integer("1"),integer("2")});matrix.rows=1;matrix.columns=2;
    return {
      {"2x",mul(integer("2"),x),0}, {"negative_coefficient",mul(integer("-3"),x),0},
      {"power",mul(integer("3"),square(x)),0},
      {"standard",node(K::Add,"",{mul(a,square(x)),mul(b,x),c}),0},
      {"4ac",node(K::Mul,"",{integer("4"),a,c}),0},
      {"coefficient_group",mul(integer("2"),plus),0},
      {"target_factorization",mul(add(mul(integer("2"),x),integer("5")),minus),0},
      {"adjacent_groups",mul(plus,minus),0},
      {"root",mul(integer("2"),node(K::Sqrt,"",{x})),0},
      {"sin",mul(integer("2"),node(K::Function,"sin",{x})),0},
      {"rational_coefficient",mul(fraction("2","3"),x),0},
      {"numeric",mul(integer("2"),integer("3")),1},
      {"mixed_number_hazard",mul(integer("2"),fraction("1","3")),1},
      {"right_negative",mul(integer("2"),integer("-3")),1},
      {"preserve_order",mul(plus,integer("2")),1},
      {"repeated",mul(x,x),1},
      {"identifier",mul(integer("2"),symbol("velocity")),1},
      {"identifier_pair",mul(symbol("ab"),symbol("cd")),1},
      {"function_application_hazard",mul(x,plus),1},
      {"decimal",mul(node(K::Decimal,"1.25"),node(K::Decimal,"2.5")),1},
      {"scientific",mul(node(K::Decimal,"1e3"),node(K::Decimal,"2e4")),1},
      {"denominator",node(K::Inv,"",{mul(integer("2"),x)}),0},
      {"half_x",mul(fraction("1","2"),x),0},
      {"numerator_denominator",node(K::Rational,"",{mul(integer("2"),x),mul(integer("3"),symbol("y"))}),0},
      {"nested_script",node(K::Pow,"",{x,node(K::Pow,"",{x,mul(integer("2"),symbol("y"))})}),0},
      {"user_D",mul(integer("2"),symbol("D")),0},
      {"unit_not_scalar_contract",mul(integer("2"),symbol("m")),1},
      {"matrix",mul(integer("2"),matrix),1},
      {"collection",mul(integer("2"),node(K::List,"",{x,symbol("y")})),1},
      {"unknown_call",mul(integer("2"),node(K::Function,"custom",{x})),1},
    };
}
} // namespace notationtest
