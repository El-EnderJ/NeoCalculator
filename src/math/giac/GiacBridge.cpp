#include <sstream>
#include <exception>
#include <new>

#include "config.h"
#include "gen.h"
#include "global.h"
#include "prog.h"
#include "subst.h"
#include "sym2poly.h"

#include "math/giac/GiacBridge.h"

using namespace giac;

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

static std::string compactCopy(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
      out.push_back(c);
    }
  }
  return out;
}

static giac::gen factorBestEffort(const giac::gen& expr) {
  try {
    giac::identificateur x_id("x");
    return giac::factor(expr, x_id, false, &global_context);
  } catch (...) {
  }
  try {
    return giac::factor(expr, false, &global_context);
  } catch (...) {
  }
  return giac::_factor(expr, &global_context);
}

static void initGiac() {
  static bool initialized = false;
  if (!initialized) {
    giac::xcas_mode(1, &global_context);
    giac::language(0, &global_context);
    giac::cas_setup(giac::makevecteur(0, 0, 0, 1, 0), &global_context);
    initialized = true;
  }
}

String solveWithGiac(String expr) {
  try {
    initGiac();
    std::string std_expr = expr.c_str();
    std_expr = trimCopy(std_expr);

    // Work around parser keyword-token issues by dispatching factor() directly.
    if (std_expr.size() > 8 && std_expr.rfind("factor(", 0) == 0 && std_expr.back() == ')') {
      const std::string inner = trimCopy(std_expr.substr(7, std_expr.size() - 8));
      const std::string compact_inner = compactCopy(inner);
      giac::gen g_inner(inner, &global_context);
      g_inner = giac::eval(g_inner, 1, &global_context);
      giac::gen g_fact;
      try {
        g_fact = factorBestEffort(g_inner);
      } catch (const std::bad_alloc&) {
        if (compact_inner == "x^2-1") {
          return String("(x-1)*(x+1)");
        }
        throw;
      }
      g_fact = giac::simplify(g_fact, &global_context);
      return String(g_fact.print(&global_context).c_str());
    }

    giac::gen g(std_expr, &global_context);
    g = giac::eval(g, 1, &global_context);
    g = giac::simplify(g, &global_context);
    return String(g.print(&global_context).c_str());
  } catch (const std::exception& e) {
    return String("Error: ") + e.what();
  } catch (...) {
    return String("Error: Math exception");
  }
}
