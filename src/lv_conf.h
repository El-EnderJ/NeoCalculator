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
 * lv_conf.h
 * Configuraci�n de LVGL 9.2 para NumOS (ESP32-S3 N16R8)
 *
 * Basado en lv_conf_template.h de LVGL 9.2.
 * Solo se habilitan los widgets y features que NumOS necesita para
 * minimizar el uso de Flash y RAM interna.
 *
 * Estrategia de memoria (ratificada por MT-02; ver
 * docs/specs/NUMOS_MEMORY_AND_PSRAM_AUDIT.md §1.1):
 *  - Firmware: LVGL 9 usa su allocator BUILTIN (TLSF) con un pool FIJO de
 *    LV_MEM_SIZE = 64 KB en `.bss` de la DRAM interna. Todos los objetos,
 *    estilos y textos LVGL compiten por esos 64 KB. NO hay PSRAM aqui.
 *  - Emulador: platformio.ini inyecta -DLV_USE_STDLIB_MALLOC=LV_STDLIB_CLIB
 *    (heap del sistema, sin pool fijo) — por eso las claves de abajo van
 *    protegidas con #ifndef.
 *  - Los draw-buffers NO salen de aqui: main.cpp los asigna con
 *    heap_caps_malloc(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA). Nunca PSRAM.
 */

#if 1  /* Set it to "1" to enable content */

#ifndef LV_CONF_H
#define LV_CONF_H

/* Guard against inclusion by the Xtensa assembler (lv_blend_helium.S defines __ASSEMBLY__) */
#ifndef __ASSEMBLY__
#include <stdint.h>
#endif

/*====================
   COLOR SETTINGS
 *====================*/

/** Profundidad de color: 16 = RGB565, perfecto para ST7789/ILI9341 */
#define LV_COLOR_DEPTH 16

/** Intercambio de bytes RGB565 (el SPI de TFT_eSPI env�a MSB primero) */
#define LV_BIG_ENDIAN_SYSTEM 0

/*====================
   MEMORY SETTINGS  (LVGL 9 — ratified by ticket MT-02)
 *====================*/

/*
 * HISTORICAL NOTE (memory audit 2026-07, §1.1): this file used to configure
 * memory through the LVGL-8 `LV_MEM_CUSTOM` macro family, routing LVGL
 * allocations to heap_caps_malloc with a 512 B internal/PSRAM cutoff. LVGL 9
 * NEVER READ those macros — that block was dead configuration on both
 * targets. The real, shipped behavior has always been LVGL 9's default:
 * the BUILTIN (TLSF) allocator over a fixed 64 KB static pool in internal
 * `.bss` (`work_mem_int` in lv_mem_core_builtin.c), no expansion.
 *
 * The keys below make that behavior EXPLICIT instead of accidental.
 * They are #ifndef-guarded so build flags keep working:
 *   - emulator_pc passes -DLV_USE_STDLIB_MALLOC=LV_STDLIB_CLIB (system heap;
 *     the 64-bit native pool exhaustion hang is documented in platformio.ini);
 *   - a scratch build may pass -DLV_MEM_SIZE=16384 to exercise the OOM path.
 *
 * Sizing rule (policy §3.5): raising LV_MEM_SIZE is allowed up to 96 KB only
 * if MT-01 probe data shows exhaustion; every +32 KB comes out of the
 * ~212 KB post-static internal heap. Moving the pool to PSRAM requires
 * hardware latency validation — do not do it casually.
 */
#ifndef LV_USE_STDLIB_MALLOC
  #define LV_USE_STDLIB_MALLOC LV_STDLIB_BUILTIN
#endif
#ifndef LV_MEM_SIZE
  #define LV_MEM_SIZE (64U * 1024U)   /* fixed internal-DRAM pool (firmware) */
#endif
#ifndef LV_MEM_POOL_EXPAND_SIZE
  #define LV_MEM_POOL_EXPAND_SIZE 0   /* no growth: exhaustion must be visible */
#endif

/*====================
   HAL / TICK
 *====================*/

/**
 * Tick custom: usa millis() de Arduino.
 * Alternativa: llamar lv_tick_inc() en un timer ISR cada 1 ms.
 */
#define LV_TICK_CUSTOM 1
#if LV_TICK_CUSTOM
  #ifdef NATIVE_SIM
    /* PC nativo: usar SDL_GetTicks() */
    #define LV_TICK_CUSTOM_INCLUDE  <SDL2/SDL.h>
    #define LV_TICK_CUSTOM_SYS_TIME_EXPR (SDL_GetTicks())
  #else
    /* ESP32: usar millis() de Arduino */
    #define LV_TICK_CUSTOM_INCLUDE  "Arduino.h"
    #define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())
  #endif
#endif

/*====================
   LOGGING
 *====================*/

/** Log de LVGL (0 = deshabilitado en produccion) */
#define LV_USE_LOG      0
#if LV_USE_LOG
    #define LV_LOG_LEVEL    LV_LOG_LEVEL_WARN
    #define LV_LOG_PRINTF   1
#endif

/*====================
   ASSERTS
 *====================*/

#define LV_USE_ASSERT_NULL      1
#define LV_USE_ASSERT_MALLOC    1
#define LV_USE_ASSERT_OBJ       0   /* Costoso en producci�n */
#define LV_USE_ASSERT_STYLE     0

/*
 * MT-02: visible failure instead of a silent hang.
 * LVGL's default LV_ASSERT_HANDLER is `while(1);` — when the fixed 64 KB
 * builtin pool is exhausted, lv_malloc() returns NULL, LV_ASSERT_MALLOC
 * fires, and the device freezes with NO serial output (risk MEMX-01; this
 * is the documented hang class from platformio.ini:170-176 and the eager
 * boot-begin() crash). The handler below:
 *   1. prints the failing LVGL source location plus an OOM hint to stdout
 *      (routed to UART0 on firmware; stderr-adjacent console natively) —
 *      no heap use, the allocator may be exactly what just failed;
 *   2. calls abort(): on the ESP32-S3 that raises a panic with a backtrace
 *      that monitor_filters=esp32_exception_decoder can decode (far more
 *      diagnostic than a spin-loop); on the emulator it fails CI fast
 *      instead of hanging until timeout.
 * NOTE: this fires for every LVGL assert (NULL-param asserts too), not just
 * pool OOM — all of those previously hung silently as well.
 * NOTE: LVGL 9.5 has no per-allocation high-water hook; pool high-water is
 * only observable via lv_mem_monitor() (surfaced by utils/MemProbe.h, MT-01).
 */
#ifndef __ASSEMBLY__
#include <stdio.h>
#include <stdlib.h>
static inline void numos_lv_assert_handler(const char *file, int line)
{
    printf("\n[LVGL] ASSERT FAILED at %s:%d — most likely lv_malloc OOM: "
           "the fixed %u-byte LVGL pool is exhausted (see lv_conf.h MEMORY "
           "SETTINGS / ticket MT-02; NULL-param asserts land here too). "
           "Aborting for a decodable backtrace.\n",
           file, line, (unsigned)LV_MEM_SIZE);
    fflush(stdout);
    abort();
}
#endif
#define LV_ASSERT_HANDLER numos_lv_assert_handler(__FILE__, __LINE__);

/*====================
   RENDIMIENTO
 *====================*/

#define LV_DRAW_COMPLEX         1   /* Sombras, gradientes, transformaciones */
#define LV_SHADOW_CACHE_SIZE    0
#define LV_IMG_CACHE_DEF_SIZE   4   /* Cach� peque�o para los 10 iconos */

/*====================
   FUENTES (Fonts)
 *====================*/

/*
 * STIX Two Math (custom LVGL font) disponible para zonas matematicas.
 * Se mantiene declarado aqui para los modulos que lo usan de forma selectiva.
 */
#ifndef __ASSEMBLY__
#ifdef __cplusplus
extern "C" {
#endif
struct _lv_font_t;
typedef struct _lv_font_t lv_font_t;
extern const lv_font_t stix_math_18;
extern const lv_font_t stix_math_12;
#ifdef __cplusplus
}
#endif
#endif

#define LV_FONT_MONTSERRAT_10   1
#define LV_FONT_MONTSERRAT_12   1
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_16   0
#define LV_FONT_MONTSERRAT_18   0
#define LV_FONT_MONTSERRAT_20   1

/** Fuente monoespaciada UNSCII 8 para el editor Python */
#define LV_FONT_UNSCII_8        1
#define LV_FONT_MONTSERRAT_22   0
#define LV_FONT_MONTSERRAT_24   0
#define LV_FONT_MONTSERRAT_28   0
#define LV_FONT_MONTSERRAT_32   0
#define LV_FONT_MONTSERRAT_36   0
#define LV_FONT_MONTSERRAT_48   0

/** Fuente que usan todos los widgets por defecto */
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#define LV_USE_FONT_PLACEHOLDER 1   /* Muestra '?' para glifos ausentes */

/*====================
   WIDGETS
   Solo los que NumOS usa activamente.
 *====================*/

#define LV_USE_ARC          1
#define LV_USE_BAR          0
#define LV_USE_BTN          1   /* Botones del grid de apps */
#define LV_USE_BTNMATRIX    0
#define LV_USE_CANVAS       0
#define LV_USE_CHECKBOX     0
#define LV_USE_DROPDOWN     1
#define LV_USE_IMG          1   /* Iconos de apps */
#define LV_USE_IMGBTN       0
#define LV_USE_KEYBOARD     0
#define LV_USE_LABEL        1   /* Nombres de apps y t�tulos */
#define LV_USE_LED          0
#define LV_USE_LINE         1   /* Requerido por widgets internos (scale) */
#define LV_USE_LIST         1   /* Stats results list */
#define LV_USE_MENU         0
#define LV_USE_MSGBOX       1   /* Error dialogs (Matrices) */
#define LV_USE_ROLLER       0
#define LV_USE_SLIDER       0
#define LV_USE_SPAN         0
#define LV_USE_SPINBOX      0
#define LV_USE_SPINNER      1
#define LV_USE_SWITCH       0
#define LV_USE_TABLE        1
#define LV_USE_CHART        1   /* Histogram (Stats) + bell curve (Probability) */
#define LV_USE_TABVIEW      0
#define LV_USE_TEXTAREA     1
#define LV_USE_TILEVIEW     0
#define LV_USE_WIN          0

/*====================
   LAYOUTS
 *====================*/

#define LV_USE_FLEX    1   /* Flex: usado internamente en las cards */
#define LV_USE_GRID    1   /* Grid: layout de 3 columnas del launcher */

/*====================
   EXTRA: GRIDNAV
   Permite navegaci�n 2D con flechas en contenedores grid.
   Imprescindible para el launcher 3-columnas + teclado f�sico.
 *====================*/
#define LV_USE_GRIDNAV  1

/*====================
   TEMAS
 *====================*/

#define LV_USE_THEME_DEFAULT    1   /* Tema base con soporte de estados */
#define LV_USE_THEME_BASIC      0
#define LV_USE_THEME_MONO       0

/*====================
   IMAGEN
 *====================*/

/** Formato RGB565 Swap: los bitmaps de Icons.h vienen de TFT_eSPI (big-endian) */
#define LV_USE_GIF      0
#define LV_USE_BMP      0
#define LV_USE_SJPG     0
#define LV_USE_PNG      0

/*====================
   MISC
 *====================*/

/** Alineaci�n de datos malloc � ESP32 requiere 4 bytes */
#define LV_ATTRIBUTE_MEM_ALIGN      __attribute__((aligned(4)))
/** No usar IRAM_ATTR aqu�: puede provocar desbordamiento de IRAM en ESP32-S3.
 *  Las funciones cr�ticas de LVGL van a flash cacheada (DROM), que es suficientemente r�pida. */
#define LV_ATTRIBUTE_FAST_MEM

/** Screenshot / monkey test � deshabilitados en producci�n */
#define LV_USE_SNAPSHOT     0
#define LV_USE_MONKEY       0

/*====================
   PERFORMANCE MONITOR
   Muestra FPS y CPU% en una esquina de la pantalla.
   Desactivar tras verificar el rendimiento (poner a 0).
 *====================*/
#define LV_USE_SYSMON            1
#if LV_USE_SYSMON
  #define LV_USE_PERF_MONITOR    0
  #if LV_USE_PERF_MONITOR
    #define LV_USE_PERF_MONITOR_POS  LV_ALIGN_BOTTOM_RIGHT
  #endif
  #define LV_USE_MEM_MONITOR     0
#endif

#endif /* LV_CONF_H */
#endif /* Enable/Disable content */


