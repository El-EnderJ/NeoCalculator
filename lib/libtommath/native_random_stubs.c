/*
 * NumOS native-build compatibility shim, added in 2026.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "tommath_private.h"

/*
 * LibTomMath's MP_HAS configuration intentionally leaves references to
 * compile-time-false random providers at -O0. Apple Clang consequently asks
 * the linker for providers that cannot be selected on macOS. These definitions
 * retain the library's original optimization level and return the same failure
 * those unavailable providers represent. The active macOS /dev/urandom path is
 * defined in s_mp_rand_platform.c and remains unchanged.
 *
 * This file compiles empty for firmware and Emscripten.
 */
#if defined(__APPLE__) && !defined(__EMSCRIPTEN__)
mp_err s_read_arc4random(void *p, size_t n)
{
   (void)p;
   (void)n;
   return MP_ERR;
}

mp_err s_read_wincsp(void *p, size_t n)
{
   (void)p;
   (void)n;
   return MP_ERR;
}

mp_err s_read_getrandom(void *p, size_t n)
{
   (void)p;
   (void)n;
   return MP_ERR;
}
#endif
