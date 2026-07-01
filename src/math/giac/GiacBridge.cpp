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

#include <sstream>
#include <exception>
#include <iostream>
#include <cctype>
#include <vector>
#include <cstring>

#include "config.h"
#include "gen.h"
#include "global.h"
#include "prog.h"
#include "subst.h"
#include "sym2poly.h"
#include "usual.h"
#include "derive.h"
#include "desolve.h"
#include "series.h"

#include "math/giac/GiacBridge.h"
#include "ui/MathSymbols.h"
#include "utils/MemProbe.h"

using namespace giac;

namespace giac {
  void check_browser_functions();
  void lexer_localization(int lang, const context * contextptr);
}

static giac::context global_context;

static std::string trimCopy(const std::string &s);

static bool isWordBoundary(char c) {
  const unsigned char uc = static_cast<unsigned char>(c);
  return !(std::isalnum(uc) || c == '_');
}

static void replaceAll(std::string &text, const std::string &from, const char *to) {
  if (from.empty()) return;
  size_t pos = 0;
  while ((pos = text.find(from, pos)) != std::string::npos) {
    text.replace(pos, from.size(), to);
    pos += std::strlen(to);
  }
}

static void replaceWordToken(std::string &text, const std::string &token, const char *to) {
  if (token.empty()) return;
  size_t pos = 0;
  while ((pos = text.find(token, pos)) != std::string::npos) {
    const bool leftOk = (pos == 0) || isWordBoundary(text[pos - 1]);
    const size_t end = pos + token.size();
    const bool rightOk = (end >= text.size()) || isWordBoundary(text[end]);
    if (leftOk && rightOk) {
      text.replace(pos, token.size(), to);
      pos += std::strlen(to);
    } else {
      pos += token.size();
    }
  }
}

static void applyDisplaySymbolMap(std::string &text) {
  // Normalize common Giac textual operators into STIX-supported glyphs.
  replaceWordToken(text, "infinity", numos::mathsym::SYMB_INFINITY);
  replaceWordToken(text, "inf", numos::mathsym::SYMB_INFINITY);
  replaceWordToken(text, "oo", numos::mathsym::SYMB_INFINITY);

  replaceAll(text, "<=", numos::mathsym::SYMB_LEQ);
  replaceAll(text, ">=", numos::mathsym::SYMB_GEQ);
  replaceAll(text, "!=", numos::mathsym::SYMB_NEQ);
  replaceAll(text, "<->", numos::mathsym::SYMB_ARROW_LR);
  replaceAll(text, "<-", numos::mathsym::SYMB_ARROW_L);
  replaceAll(text, "->", numos::mathsym::SYMB_ARROW_R);
}

static bool containsRootofText(const std::string &s) {
  return s.find("rootof(") != std::string::npos;
}

static bool startsWithIgnoreCase(const std::string &s, const char *prefix) {
  const size_t n = std::char_traits<char>::length(prefix);
  if (s.size() < n) return false;
  for (size_t i = 0; i < n; ++i) {
    unsigned char a = (unsigned char)s[i];
    unsigned char b = (unsigned char)prefix[i];
    if (std::tolower(a) != std::tolower(b)) return false;
  }
  return true;
}

static std::string mapPresentationCommandAliases(const std::string &expr) {
  std::string trimmed = trimCopy(expr);

  struct TrigAlias {
    const char *name;
  };
  static const TrigAlias kTrigAliases[] = {
    {"trigsimplify("},
    {"trigsimp("},
    {"trig_simplify("},
    {"simplifytrig("}
  };

  for (size_t i = 0; i < sizeof(kTrigAliases) / sizeof(kTrigAliases[0]); ++i) {
    const size_t prefixLen = std::char_traits<char>::length(kTrigAliases[i].name);
    if (startsWithIgnoreCase(trimmed, kTrigAliases[i].name) && trimmed.size() > prefixLen && trimmed.back() == ')') {
      const std::string inner = trimmed.substr(prefixLen, trimmed.size() - prefixLen - 1);
      return "simplify(texpand(" + inner + "))";
    }
  }

  struct Alias {
    const char *from;
    const char *to;
  };
  static const Alias kAliases[] = {
    {"trigexpand(", "texpand("}
  };

  for (size_t i = 0; i < sizeof(kAliases) / sizeof(kAliases[0]); ++i) {
    const size_t fromLen = std::char_traits<char>::length(kAliases[i].from);
    if (startsWithIgnoreCase(trimmed, kAliases[i].from)) {
      return std::string(kAliases[i].to) + trimmed.substr(fromLen);
    }
  }
  return trimmed;
}

static giac::gen evalInBridgeContext(const std::string &expr) {
  giac::gen g(expr, &global_context);
  return giac::eval(g, giac::eval_level(&global_context), &global_context);
}

static giac::gen prettifyRootofIfNeeded(const giac::gen &input) {
  std::string printed = input.print(&global_context);
  if (!containsRootofText(printed)) return input;

  giac::gen best = input;

  try {
    giac::gen candidate = evalInBridgeContext("normal(" + printed + ")");
    if (!is_undef(candidate)) {
      best = candidate;
      printed = best.print(&global_context);
    }
  } catch (...) {
    // Keep original if normalization fails.
  }

  if (containsRootofText(printed)) {
    try {
      giac::gen candidate = evalInBridgeContext("radsimp(" + printed + ")");
      if (!is_undef(candidate)) {
        best = candidate;
        printed = best.print(&global_context);
      }
    } catch (...) {
      // Keep last valid representation.
    }
  }

  if (containsRootofText(printed)) {
    try {
      giac::gen candidate = giac::evalf(best, 1, &global_context);
      if (!is_undef(candidate)) {
        best = candidate;
      }
    } catch (...) {
      // Keep symbolic form if numeric approximation fails.
    }
  }

  return best;
}

static void initGiac() {
  static bool initialized = false;
  if (!initialized) {
    giac::xcas_mode(0, &global_context);
    giac::approx_mode(false, &global_context);
    giac::complex_mode(false, &global_context);
    giac::complex_variables(false, &global_context);
    giac::i_sqrt_minus1(1, &global_context);
    giac::withsqrt(true, &global_context);
    giac::eval_level(&global_context) = 1;
    giac::step_infolevel(&global_context) = 0;
    // This Giac snapshot does not expose symbolic_mode(...); keep symbolic behavior
    // via exact evaluation level and approx_mode(false).
    giac::language(0, &global_context);
    giac::check_browser_functions();
    giac::lexer_localization(0, &global_context);
    giac::cas_setup(giac::makevecteur(0, 0, 0, 1, 0), &global_context);
    initialized = true;
  }
}

static std::string trimCopy(const std::string &s) {
  size_t begin = 0;
  while (begin < s.size() && std::isspace((unsigned char)s[begin])) ++begin;
  size_t end = s.size();
  while (end > begin && std::isspace((unsigned char)s[end - 1])) --end;
  return s.substr(begin, end - begin);
}

static bool isIdentifier(const std::string &s) {
  if (s.empty()) return false;
  if (!(std::isalpha((unsigned char)s[0]) || s[0] == '_')) return false;
  for (size_t i = 1; i < s.size(); ++i) {
    unsigned char c = (unsigned char)s[i];
    if (!(std::isalnum(c) || c == '_')) return false;
  }
  return true;
}

static void replaceIdentifierToken(std::string &s, const std::string &token, const std::string &replacement) {
  size_t pos = 0;
  while (true) {
    pos = s.find(token, pos);
    if (pos == std::string::npos) break;
    bool left_ok = (pos == 0) || (!std::isalnum((unsigned char)s[pos - 1]) && s[pos - 1] != '_');
    size_t after = pos + token.size();
    bool right_ok = (after >= s.size()) || (!std::isalnum((unsigned char)s[after]) && s[after] != '_');
    if (left_ok && right_ok) {
      s.replace(pos, token.size(), replacement);
      pos += replacement.size();
    } else {
      pos += token.size();
    }
  }
}

static size_t findTopLevelChar(const std::string &s, char target) {
  int depth = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    char c = s[i];
    if (c == '(' || c == '[' || c == '{') ++depth;
    else if ((c == ')' || c == ']' || c == '}') && depth > 0) --depth;
    else if (c == target && depth == 0) return i;
  }
  return std::string::npos;
}

static void normalizeDesolvePrimeNotation(std::string &s) {
  if (s.rfind("desolve(", 0) != 0) return;
  size_t open = s.find('(');
  size_t close = s.rfind(')');
  if (open == std::string::npos || close == std::string::npos || close <= open) return;

  std::string body = s.substr(open + 1, close - open - 1);
  size_t comma = findTopLevelChar(body, ',');
  if (comma == std::string::npos) return;

  std::string eq = trimCopy(body.substr(0, comma));
  std::string y = trimCopy(body.substr(comma + 1));
  if (!isIdentifier(y)) return;

  size_t equal = findTopLevelChar(eq, '=');
  if (equal == std::string::npos) return;
  std::string left = trimCopy(eq.substr(0, equal));
  std::string right = trimCopy(eq.substr(equal + 1));

  if (left != (y + "'")) return;

  replaceIdentifierToken(right, y, y + "(x)");
  s = "desolve(diff(" + y + "(x),x)=" + right + "," + y + "(x))";
}

static bool extractDesolvePrimeCall(const std::string &s, std::string &y, std::string &rhs) {
  if (s.rfind("desolve(", 0) != 0) return false;
  size_t open = s.find('(');
  size_t close = s.rfind(')');
  if (open == std::string::npos || close == std::string::npos || close <= open) return false;

  std::string body = s.substr(open + 1, close - open - 1);
  size_t comma = findTopLevelChar(body, ',');
  if (comma == std::string::npos) return false;

  std::string eq = trimCopy(body.substr(0, comma));
  y = trimCopy(body.substr(comma + 1));
  if (!isIdentifier(y)) return false;

  size_t equal = findTopLevelChar(eq, '=');
  if (equal == std::string::npos) return false;
  std::string left = trimCopy(eq.substr(0, equal));
  rhs = trimCopy(eq.substr(equal + 1));
  return left == (y + "'");
}

static bool startsWith(const std::string &s, const char *prefix) {
  const size_t n = std::char_traits<char>::length(prefix);
  return s.size() >= n && s.compare(0, n, prefix) == 0;
}

static bool splitTopLevelArgs(const std::string &s, std::vector<std::string> &args) {
  args.clear();
  int depth = 0;
  size_t start = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    char c = s[i];
    if (c == '(' || c == '[' || c == '{') ++depth;
    else if ((c == ')' || c == ']' || c == '}') && depth > 0) --depth;
    else if (c == ',' && depth == 0) {
      args.push_back(trimCopy(s.substr(start, i - start)));
      start = i + 1;
    }
  }
  args.push_back(trimCopy(s.substr(start)));
  return !args.empty();
}

static bool parseFunctionCallArgs(const std::string &s, const char *name, std::vector<std::string> &args) {
  const size_t n = std::char_traits<char>::length(name);
  if (s.size() < n + 2 || s.compare(0, n, name) != 0) return false;

  size_t open = n;
  while (open < s.size() && std::isspace((unsigned char)s[open])) ++open;
  if (open >= s.size() || s[open] != '(' || s.back() != ')') return false;

  return splitTopLevelArgs(s.substr(open + 1, s.size() - open - 2), args);
}

static bool parseInfinityAliasDirection(const std::string &token, int &directionHint) {
  std::string t = trimCopy(token);
  std::string norm;
  norm.reserve(t.size());
  for (size_t i = 0; i < t.size(); ++i) {
    unsigned char c = (unsigned char)t[i];
    if (!std::isspace(c)) norm.push_back((char)std::tolower(c));
  }

  if (norm == "oo" || norm == "+oo" || norm == "inf" || norm == "+inf" ||
      norm == "infty" || norm == "+infty" || norm == "infinity" || norm == "+infinity") {
    directionHint = 1;
    return true;
  }
  if (norm == "-oo" || norm == "-inf" || norm == "-infty" || norm == "-infinity") {
    directionHint = -1;
    return true;
  }
  directionHint = 0;
  return false;
}

static giac::gen makeUnsignedInfinity() {
  return giac::gen(giac::identificateur("infinity"));
}

static bool isSquareNumericMatrix(const giac::gen &g) {
  if (g.type != _VECT || !g._VECTptr || g._VECTptr->empty()) return false;
  const size_t n = g._VECTptr->size();
  for (size_t i = 0; i < n; ++i) {
    const giac::gen &row = (*g._VECTptr)[i];
    if (row.type != _VECT || !row._VECTptr || row._VECTptr->size() != n) return false;
  }
  return true;
}

static giac::gen diagonalToVector(const giac::gen &g) {
  if (!isSquareNumericMatrix(g)) return g;
  const size_t n = g._VECTptr->size();
  giac::vecteur diag;
  diag.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    const giac::vecteur &row = *(*g._VECTptr)[i]._VECTptr;
    for (size_t j = 0; j < n; ++j) {
      if (i != j && !is_zero(row[j], &global_context)) return g;
    }
    diag.push_back(row[i]);
  }
  return giac::gen(diag, _VECT);
}

String solveWithGiac(String expr) {
  // MT-01: Giac is the largest uncapped allocator in the system (audit §5.6)
  // and runs on the shared 64 KB loopTask stack. Probe the pre/post deltas
  // (PSRAM footprint per query, stack high-water) at the single entry point.
  NUMOS_MEM_PROBE("giac-pre");
  try {
    initGiac();
    std::string std_expr = expr.c_str();

    // Serial commands may come prefixed with ':'; strip it for semantic parsing.
    std_expr = trimCopy(std_expr);
    if (!std_expr.empty() && std_expr[0] == ':') {
      std_expr = trimCopy(std_expr.substr(1));
    }

    std_expr = mapPresentationCommandAliases(std_expr);

    std::ostringstream step_buf;
    giac::gen g;
    {
      // Capture textual traces from cout/cerr and Giac logptr.
      std::streambuf* old_cout = std::cout.rdbuf(step_buf.rdbuf());
      std::streambuf* old_cerr = std::cerr.rdbuf(step_buf.rdbuf());
      std::ostream *old_log = giac::logptr(&global_context);
      giac::logptr(&step_buf, &global_context);

      g = giac::gen(std_expr, &global_context);
      g = giac::eval(g, giac::eval_level(&global_context), &global_context);

      g = prettifyRootofIfNeeded(g);

      // Canonicalize egvl diagonal-matrix output to eigenvalue list.
      if (startsWith(std_expr, "egvl(")) {
        g = diagonalToVector(g);
      }

      giac::logptr(old_log, &global_context);
      std::cout.rdbuf(old_cout);
      std::cerr.rdbuf(old_cerr);
    }

    std::string result = g.print(&global_context);
    applyDisplaySymbolMap(result);
    std::string steps = step_buf.str();
    if (!steps.empty()) {
      applyDisplaySymbolMap(steps);
      result += "\n[STEP_OUTPUT]\n";
      result += steps;
    }
    NUMOS_MEM_PROBE("giac-post");
    return String(result.c_str());
  } catch (const std::exception& e) {
    NUMOS_MEM_PROBE("giac-post-ex");
    return String("Error: ") + e.what();
  } catch (...) {
    NUMOS_MEM_PROBE("giac-post-ex");
    return String("Error: Math exception");
  }
}
