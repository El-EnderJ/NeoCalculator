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
 * Estrategia de memoria:
 *  - LVGL usa la PSRAM como heap a trav�s de heap_caps_malloc().
 *  - Los draw-buffers se asignan tambi�n en PSRAM desde main.cpp.
 *  - Los objetos y estilos LVGL vivir�n en PSRAM liberando la SRAM
 *    interna para el Math Engine (stacks, variables de c�lculo, etc.)
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
   MEMORY SETTINGS
 *====================*/

/**
 * Allocator personalizado ? heap_caps_malloc en PSRAM.
 * Requiere que BOARD_HAS_PSRAM est� definido (ver platformio.ini).
 */
#define LV_MEM_CUSTOM 1
#if LV_MEM_CUSTOM
  #ifdef NATIVE_SIM
    /* PC nativo: usar malloc/realloc/free est�ndar */
    #define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
    #define LV_MEM_CUSTOM_ALLOC(size)        malloc(size)
    #define LV_MEM_CUSTOM_REALLOC(ptr, size) realloc((ptr), (size))
    #define LV_MEM_CUSTOM_FREE(ptr)          free(ptr)
  #else
    /*
     * ESP32 allocator policy:
     * - Small LVGL metadata blocks (objects, child arrays, styles) in INTERNAL RAM.
     * - Large blocks in PSRAM to preserve internal contiguous space.
     */
    #define LV_MEM_CUSTOM_INCLUDE <esp_heap_caps.h>
    #define LV_MEM_INTERNAL_CUTOFF 512U
    #define LV_MEM_CUSTOM_ALLOC(size) \
      (((size) <= LV_MEM_INTERNAL_CUTOFF) \
        ? heap_caps_malloc((size), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) \
        : heap_caps_malloc((size), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT))
    #define LV_MEM_CUSTOM_REALLOC(ptr, size) \
      (((size) <= LV_MEM_INTERNAL_CUTOFF) \
        ? heap_caps_realloc((ptr), (size), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) \
        : heap_caps_realloc((ptr), (size), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT))
    #define LV_MEM_CUSTOM_FREE(ptr) free(ptr)
  #endif
#else
    /* Fallback: allocator interno de LVGL (no se usa cuando LV_MEM_CUSTOM=1) */
    #define LV_MEM_SIZE (64U * 1024U)
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


