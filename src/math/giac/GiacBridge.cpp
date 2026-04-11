#include <sstream>
#include <exception>
#include <cstring>

#include "config.h"
#include "gen.h"
#include "global.h"
#include "prog.h"
#include "subst.h"
#include "sym2poly.h"

#include "math/giac/GiacBridge.h"

using namespace giac;

namespace giac {
  void check_browser_functions();
  void lexer_localization(int lang, const context * contextptr);
}

static giac::context global_context;

static std::string trimCopy(const std::string& s) {
  const char* ws = " \t\r\n";
  const size_t begin = s.find_first_not_of(ws);
  if (begin == std::string::npos) {
    return std::string();
  }
  const size_t end = s.find_last_not_of(ws);
  return s.substr(begin, end - begin + 1);
}

static bool startsWithCall(const std::string& expr, const char* fn) {
  const size_t fn_len = std::strlen(fn);
  return expr.size() > fn_len + 1 && expr.compare(0, fn_len, fn) == 0 && expr[fn_len] == '(' && expr.back() == ')';
}

static bool preserveStructureResult(const std::string& expr) {
  return startsWithCall(expr, "factor") ||
         startsWithCall(expr, "cfactor") ||
         startsWithCall(expr, "partfrac") ||
         startsWithCall(expr, "cpartfrac");
}

static giac::gen factorBestEffort(const giac::gen& expr) {
  const giac::identificateur x_id("x");
  try {
    giac::gen r = giac::factor(expr, x_id, false, &global_context);
    if (!(r == expr)) return r;
  } catch (...) {
  }
  try {
    giac::gen r = giac::factor(expr, false, &global_context);
    if (!(r == expr)) return r;
  } catch (...) {
  }
  try {
    giac::gen r = giac::factor(expr, x_id, true, &global_context);
    if (!(r == expr)) return r;
  } catch (...) {
  }
  try {
    giac::gen r = giac::factor(expr, true, &global_context);
    if (!(r == expr)) return r;
  } catch (...) {
  }
  try {
    return giac::_factor(expr, &global_context);
  } catch (...) {
  }
  return expr;
}

static void initGiac() {
  static bool initialized = false;
  if (!initialized) {
    giac::xcas_mode(0, &global_context);
    giac::approx_mode(false, &global_context);
    giac::complex_mode(false, &global_context);
    giac::withsqrt(true, &global_context);
    giac::language(0, &global_context);
    giac::check_browser_functions();
    giac::lexer_localization(0, &global_context);
    giac::cas_setup(giac::makevecteur(0, 0, 0, 1, 0), &global_context);
    initialized = true;
  }
}

String solveWithGiac(String expr) {
  try {
    initGiac();
    std::string std_expr = expr.c_str();
    std_expr = trimCopy(std_expr);
    const bool keep_structure = preserveStructureResult(std_expr);

    // Work around parser keyword-token issues by dispatching factor() directly.
    if (startsWithCall(std_expr, "factor")) {
      const std::string inner = trimCopy(std_expr.substr(7, std_expr.size() - 8));
      giac::gen g_inner(inner, &global_context);
      g_inner = giac::eval(g_inner, 1, &global_context);
      giac::gen g_fact;
      try {
        g_fact = giac::_factor(g_inner, &global_context);
      } catch (...) {
        g_fact = factorBestEffort(g_inner);
      }
      return String(g_fact.print(&global_context).c_str());
    }

    giac::gen g(std_expr, &global_context);
    g = giac::eval(g, 1, &global_context);
    if (!keep_structure) {
      g = giac::simplify(g, &global_context);
    }
    return String(g.print(&global_context).c_str());
  } catch (const std::exception& e) {
    return String("Error: ") + e.what();
  } catch (...) {
    return String("Error: Math exception");
  }
}
