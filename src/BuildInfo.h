/*
 * NeoCalculator - NumOS
 * Copyright (C) 2026 Juan Ramon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

/**
 * BuildInfo.h — read-only build identity (commit, dirty flag, PlatformIO env).
 *
 * Values come from the header that scripts/build_metadata.py generates in the
 * build directory; a build without that script reports "unknown". Every
 * accessor returns a string literal with static storage: no heap, safe to use
 * with lv_label_set_text_static().
 */

#pragma once

namespace numos::build {

const char* commit();       ///< full commit hash, or "unknown"
const char* shortCommit();  ///< 7-character hash, or "unknown"
bool dirty();               ///< working tree not clean when the build started
const char* id();           ///< "<short>" or "<short>-dirty"
const char* environment();  ///< PlatformIO env name, or "unknown"
/** "[BOOT] numos commit=<id> env=<env>" (no trailing newline). */
const char* bootLine();

}  // namespace numos::build
