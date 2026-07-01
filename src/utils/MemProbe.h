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

/**
 * MemProbe.h — Memory diagnostics probe (ticket MT-01, policy §6).
 *
 * One grep-stable serial line exposing the memory state of every domain
 * that matters on the ESP32-S3 (see NUMOS_MEMORY_AND_PSRAM_AUDIT.md §3):
 *
 *   [MEM] <tag> int=<free>/<largest> psram=<free>/<largest> low=<int>/<psram> lvgl=<used%>/<max%> stack=<words>
 *
 *   int    = internal DRAM heap: free bytes / largest free block
 *   psram  = SPIRAM heap:        free bytes / largest free block
 *   low    = lifetime minimum free (low-water) for internal / PSRAM
 *   lvgl   = builtin 64 KB LVGL pool: current used % / high-water used %
 *            (omitted when LVGL uses the CLIB allocator — emulator)
 *   stack  = uxTaskGetStackHighWaterMark of the calling task, in words
 *            (minimum stack headroom ever observed — smaller is worse)
 *
 * Contract (MT-01):
 *   - The probe itself allocates nothing: fixed char[160] on the stack,
 *     printed directly to the NumOS serial backend.
 *   - Native/emulator builds compile it to a no-op: no ESP heap APIs are
 *     ever referenced outside ARDUINO (the emulator runs libc malloc and
 *     has no PSRAM/heap_caps — audit §7).
 *   - Probes belong at lifecycle boundaries only (boot, app enter/exit,
 *     Giac pre/post, heartbeat) — never inside per-frame UI loops.
 *
 * Enable/disable with ONE flag: -DNUMOS_MEM_PROBE_ENABLE=0 silences every
 * probe site at compile time (default: enabled on firmware).
 */

#pragma once

#ifndef NUMOS_MEM_PROBE_ENABLE
  #ifdef ARDUINO
    #define NUMOS_MEM_PROBE_ENABLE 1
  #else
    #define NUMOS_MEM_PROBE_ENABLE 0   /* native sim: always a no-op */
  #endif
#endif

#if NUMOS_MEM_PROBE_ENABLE && defined(ARDUINO)

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <cstdio>
#include "input/NumosSerialBackend.h"

namespace numos {

inline void memProbe(const char* tag) {
    const size_t intFree  = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const size_t intLarge = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    const size_t intLow   = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    const size_t psFree   = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    const size_t psLarge  = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    const size_t psLow    = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
    const unsigned stackHW =
        (unsigned)uxTaskGetStackHighWaterMark(nullptr);

    char line[160];
    int n = snprintf(line, sizeof(line),
                     "[MEM] %s int=%u/%u psram=%u/%u low=%u/%u",
                     tag,
                     (unsigned)intFree, (unsigned)intLarge,
                     (unsigned)psFree,  (unsigned)psLarge,
                     (unsigned)intLow,  (unsigned)psLow);

#if LV_USE_STDLIB_MALLOC == LV_STDLIB_BUILTIN
    // LVGL builtin TLSF pool stats. LVGL 9.5 exposes current usage and the
    // pool high-water (max_used) via lv_mem_monitor(); there is no finer
    // per-allocation high-water hook (MT-02 documented limitation).
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    const unsigned maxPct = mon.total_size
        ? (unsigned)((uint64_t)mon.max_used * 100u / mon.total_size) : 0u;
    if (n > 0 && n < (int)sizeof(line)) {
        n += snprintf(line + n, sizeof(line) - n, " lvgl=%u%%/%u%%",
                      (unsigned)mon.used_pct, maxPct);
    }
#endif

    if (n > 0 && n < (int)sizeof(line)) {
        snprintf(line + n, sizeof(line) - n, " stack=%u", stackHW);
    }
    NUMOS_SERIAL.println(line);
}

} // namespace numos

#define NUMOS_MEM_PROBE(tag) ::numos::memProbe(tag)

#else  // probes disabled or native build

#define NUMOS_MEM_PROBE(tag) ((void)0)

#endif // NUMOS_MEM_PROBE_ENABLE && ARDUINO
