/*
 * NeoCalculator - NumOS
 * Copyright (C) 2026 Juan Ramon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

// The only translation unit that sees the generated identity header, so a new
// commit recompiles this file alone.

#include "BuildInfo.h"

#if defined(__has_include)
#if __has_include("numos_build_info.h")
#include "numos_build_info.h"
#endif
#endif

#ifndef NUMOS_BUILD_COMMIT
#define NUMOS_BUILD_COMMIT "unknown"
#endif
#ifndef NUMOS_BUILD_COMMIT_SHORT
#define NUMOS_BUILD_COMMIT_SHORT "unknown"
#endif
#ifndef NUMOS_BUILD_DIRTY
#define NUMOS_BUILD_DIRTY 0
#endif
#ifndef NUMOS_BUILD_ID
#define NUMOS_BUILD_ID NUMOS_BUILD_COMMIT_SHORT
#endif
#ifndef NUMOS_BUILD_ENVIRONMENT
#define NUMOS_BUILD_ENVIRONMENT "unknown"
#endif

namespace numos::build {

const char* commit() { return NUMOS_BUILD_COMMIT; }
const char* shortCommit() { return NUMOS_BUILD_COMMIT_SHORT; }
bool dirty() { return NUMOS_BUILD_DIRTY != 0; }
const char* id() { return NUMOS_BUILD_ID; }
const char* environment() { return NUMOS_BUILD_ENVIRONMENT; }
const char* bootLine() {
    return "[BOOT] numos commit=" NUMOS_BUILD_ID " env=" NUMOS_BUILD_ENVIRONMENT;
}

}  // namespace numos::build
