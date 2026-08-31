/*
 * NumOS compatibility file added in 2026 for vendored Giac/KhiCAS integration.
 * See ../NUMOS_CHANGES.md for provenance and the file-by-file history.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

// Minimal non-TI console compatibility layer for embedded builds.
#ifndef LCD_WIDTH_PX
#define LCD_WIDTH_PX 320
#endif

#ifndef LCD_HEIGHT_PX
#define LCD_HEIGHT_PX 240
#endif

#ifndef STATUS_AREA_PX
#define STATUS_AREA_PX 0
#endif

#ifndef COLOR_BLACK
#define COLOR_BLACK 0
#endif

#ifndef COLOR_WHITE
#define COLOR_WHITE 65535
#endif
