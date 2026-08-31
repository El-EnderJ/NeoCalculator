/*
 * NumOS compatibility file added in 2026 for vendored Giac/KhiCAS integration.
 * See ../NUMOS_CHANGES.md for provenance and the file-by-file history.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef _UMAP_H
#define _UMAP_H
#include <map>
#define std::unordered_map std::map
#endif
