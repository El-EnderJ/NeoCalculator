/*
 * NumOS compatibility file added in 2026 for vendored Giac/KhiCAS integration.
 * See ../NUMOS_CHANGES.md for provenance and the file-by-file history.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

typedef char Char;

#ifndef TEXT_MODE_NORMAL
#define TEXT_MODE_NORMAL 0
#endif

#ifndef TEXT_COLOR_BLACK
#define TEXT_COLOR_BLACK 0
#endif

// Minimal TI CSDK compatibility declarations for non-TI targets.
#ifndef COLOR_BLACK
#define COLOR_BLACK 0
#endif

#ifndef COLOR_WHITE
#define COLOR_WHITE 65535
#endif

#ifndef LCD_WIDTH_PX
#define LCD_WIDTH_PX 320
#endif

#ifndef LCD_HEIGHT_PX
#define LCD_HEIGHT_PX 240
#endif

// Some sources alias PrintMini to Printmini.
static inline int Printmini(int, int, const char *, int, int) { return 0; }

// Minimal RTC/time stubs used by Giac legacy code paths.
static int rtc_Minutes = 0;
static int rtc_Seconds = 0;
static inline void set_time(int, int) {}
static inline unsigned long millis() { return 0; }
static inline void os_wait_1ms(int) {}

#ifndef KEY_CTRL_EXIT
#define KEY_CTRL_EXIT 5
#endif

#ifndef KEY_CTRL_EXE
#define KEY_CTRL_EXE 4
#endif

#ifndef KEY_CTRL_LEFT
#define KEY_CTRL_LEFT 0
#endif

#ifndef KEY_CTRL_RIGHT
#define KEY_CTRL_RIGHT 3
#endif

#ifndef KEY_CTRL_UP
#define KEY_CTRL_UP 1
#endif

#ifndef KEY_CTRL_DOWN
#define KEY_CTRL_DOWN 2
#endif

#ifndef KEY_CTRL_F1
#define KEY_CTRL_F1 10
#endif

#ifndef KEY_CTRL_F2
#define KEY_CTRL_F2 11
#endif

#ifndef KEY_CTRL_F3
#define KEY_CTRL_F3 12
#endif

#ifndef KEY_CTRL_F4
#define KEY_CTRL_F4 13
#endif

#ifndef KEY_CTRL_F5
#define KEY_CTRL_F5 14
#endif

#ifndef KEY_CTRL_F6
#define KEY_CTRL_F6 15
#endif

#ifndef KEY_CTRL_VARS
#define KEY_CTRL_VARS 16
#endif

#ifndef KEY_CTRL_XTT
#define KEY_CTRL_XTT 17
#endif

#ifndef KEY_CTRL_MIXEDFRAC
#define KEY_CTRL_MIXEDFRAC 18
#endif

#ifndef KEY_CTRL_FRACCNVRT
#define KEY_CTRL_FRACCNVRT 19
#endif

#ifndef KEY_CTRL_FD
#define KEY_CTRL_FD 20
#endif

#ifndef KEY_CTRL_INS
#define KEY_CTRL_INS 21
#endif

#ifndef KEY_CHAR_PLUS
#define KEY_CHAR_PLUS 100
#endif

#ifndef KEY_CHAR_MINUS
#define KEY_CHAR_MINUS 101
#endif

#ifndef KEY_CHAR_PMINUS
#define KEY_CHAR_PMINUS 102
#endif

#ifndef KEY_CHAR_MULT
#define KEY_CHAR_MULT 103
#endif

#ifndef KEY_CHAR_FRAC
#define KEY_CHAR_FRAC 104
#endif

#ifndef KEY_CHAR_DIV
#define KEY_CHAR_DIV 105
#endif

#ifndef KEY_CHAR_POW
#define KEY_CHAR_POW 106
#endif

#ifndef KEY_CHAR_ROOT
#define KEY_CHAR_ROOT 107
#endif

#ifndef KEY_CHAR_SQUARE
#define KEY_CHAR_SQUARE 108
#endif

#ifndef KEY_CHAR_CUBEROOT
#define KEY_CHAR_CUBEROOT 109
#endif

#ifndef KEY_CHAR_POWROOT
#define KEY_CHAR_POWROOT 110
#endif

#ifndef KEY_CHAR_RECIP
#define KEY_CHAR_RECIP 111
#endif

#ifndef KEY_CHAR_THETA
#define KEY_CHAR_THETA 112
#endif

#ifndef KEY_CHAR_VALR
#define KEY_CHAR_VALR 113
#endif

#ifndef KEY_CHAR_ANGLE
#define KEY_CHAR_ANGLE 114
#endif

#ifndef KEY_CHAR_LN
#define KEY_CHAR_LN 115
#endif

#ifndef KEY_CHAR_LOG
#define KEY_CHAR_LOG 116
#endif

#ifndef KEY_CHAR_EXPN10
#define KEY_CHAR_EXPN10 117
#endif

#ifndef KEY_CHAR_EXPN
#define KEY_CHAR_EXPN 118
#endif

#ifndef KEY_CHAR_SIN
#define KEY_CHAR_SIN 119
#endif

#ifndef KEY_CHAR_COS
#define KEY_CHAR_COS 120
#endif

#ifndef KEY_CHAR_TAN
#define KEY_CHAR_TAN 121
#endif

#ifndef KEY_CHAR_ASIN
#define KEY_CHAR_ASIN 122
#endif

#ifndef KEY_CHAR_ACOS
#define KEY_CHAR_ACOS 123
#endif

#ifndef KEY_CHAR_ATAN
#define KEY_CHAR_ATAN 124
#endif

#ifndef KEY_CHAR_STORE
#define KEY_CHAR_STORE 125
#endif

#ifndef KEY_CHAR_IMGNRY
#define KEY_CHAR_IMGNRY 126
#endif

#ifndef KEY_CHAR_PI
#define KEY_CHAR_PI 127
#endif

#ifndef KEY_CHAR_EXP
#define KEY_CHAR_EXP 128
#endif

#ifndef KEY_CHAR_ANS
#define KEY_CHAR_ANS 129
#endif

#ifndef STATUS_AREA_PX
#define STATUS_AREA_PX 24
#endif

#ifndef COLOR_SELECTED
#define COLOR_SELECTED 31695
#endif

#ifndef C58
#define C58 58
#endif

#ifndef C85
#define C85 85
#endif

#ifndef MINI_REV
#define MINI_REV 0
#endif

#ifndef MENU_RETURN_SELECTION
#define MENU_RETURN_SELECTION 1
#endif

typedef struct MenuItem {
  char *text;
} MenuItem;

typedef struct Menu {
  int numitems;
  MenuItem *items;
  int height;
  int scrollbar;
  int scrollout;
  char *title;
  int selection;
} Menu;

static inline void os_fill_rect(int, int, int, int, int) {}
static inline void os_draw_string_medium(int, int, int, int, const char *, int) {}
static inline void Bdisp_AllClr_VRAM() {}
static inline void dConsoleRedraw() {}
static inline void os_set_pixel(int, int, unsigned short) {}
static inline unsigned short os_get_pixel(int, int) { return 0; }

static inline int iskeydown(int) { return 0; }
static inline void GetKey(int *key) {
  if (key) {
    *key = KEY_CTRL_EXE;
  }
}

static inline void ck_getkey(int *key) { GetKey(key); }

static inline int Printmini(int, int, const char *, int) { return 0; }

#ifndef PrintMini
#define PrintMini Printmini
#endif

static inline void PrintXY(int, int, const Char *, int) {}
static inline void Bdisp_PutDisp_DD() {}
static inline void clear_screen() {}

static inline int get_free_memory() { return 0; }
static inline int doMenu(Menu *menu) {
  if (menu) {
    menu->selection = menu->numitems > 0 ? 1 : 0;
  }
  return MENU_RETURN_SELECTION;
}

static inline int showCatalog(char *text, int) {
  if (text) {
    text[0] = 0;
  }
  return 0;
}

static inline unsigned char Setup_GetEntry(unsigned int) { return 0; }
static inline char *Setup_SetEntry(unsigned int, char) { return nullptr; }

#ifndef KEY_CTRL_DEL
#define KEY_CTRL_DEL 22
#endif

#ifndef KEY_CTRL_AC
#define KEY_CTRL_AC 23
#endif

#ifndef KEY_CTRL_PASTE
#define KEY_CTRL_PASTE 24
#endif

#ifndef KEY_CTRL_PRGM
#define KEY_CTRL_PRGM 25
#endif

#ifndef KEY_CTRL_CATALOG
#define KEY_CTRL_CATALOG 26
#endif

#ifndef KEY_CTRL_OPTN
#define KEY_CTRL_OPTN 27
#endif

#ifndef KEY_CTRL_QUIT
#define KEY_CTRL_QUIT 28
#endif

#ifndef KEY_CTRL_SETUP
#define KEY_CTRL_SETUP 29
#endif

#ifndef KEY_CHAR_COMMA
#define KEY_CHAR_COMMA 130
#endif

#ifndef KEY_CHAR_LPAR
#define KEY_CHAR_LPAR 131
#endif

#ifndef KEY_CHAR_RPAR
#define KEY_CHAR_RPAR 132
#endif

#ifndef KEY_CHAR_LBRCKT
#define KEY_CHAR_LBRCKT 133
#endif

#ifndef KEY_CHAR_RBRCKT
#define KEY_CHAR_RBRCKT 134
#endif

#ifndef KEY_CHAR_LBRACE
#define KEY_CHAR_LBRACE 135
#endif

#ifndef KEY_CHAR_RBRACE
#define KEY_CHAR_RBRACE 136
#endif

#ifndef KEY_CHAR_EQUAL
#define KEY_CHAR_EQUAL 137
#endif

#ifndef KEY_CHAR_MAT
#define KEY_CHAR_MAT 138
#endif

#ifndef KEY_CHAR_LIST
#define KEY_CHAR_LIST 139
#endif
