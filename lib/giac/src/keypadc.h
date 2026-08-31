/*
 * NumOS compatibility file added in 2026 for vendored Giac/KhiCAS integration.
 * See ../NUMOS_CHANGES.md for provenance and the file-by-file history.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <stdint.h>

// Minimal keypad state placeholder for non-TI builds.
static uint8_t kb_Data[8] = {0};
