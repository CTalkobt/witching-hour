/* hw.h — registers, the memory map and the screen layout
 *
 * cc45's plain char/int/long are unsigned: signed values say so.
 */
#pragma once

#define REG(a) (*(volatile unsigned char *)(a))

/* memory (linear addresses) */
#define GFX1_LIN   0x0040000UL       /* WITCHGFX1: the characters (bank 4) */
#define GFX2_LIN   0x0050000UL       /* WITCHGFX2: sprites, maps, samples (bank 5) */

#define COLOR_LIN  0xFF80000UL       /* colour RAM: 2 bytes a cell */

/* the screen: 320 x 200, 25 rows of 41 cells (one extra for the fine scroll) */
#define COLS       40
#define LCOLS      41                /* cells a row (CHRCOUNT) */
#define ROWS       25
#define LINESTEP   (LCOLS * 2)

/* the play area, in screen pixels: x 0-319, y 8-199 (row 0 is the HUD) */
#define PLAY_TOP   10
#define PLAY_BOT   192

/* the play area is 304 pixels wide (the character plane's x 8-311) */
#define SCREEN_W   304
/* sprites: VIC-II coordinates. Screen pixel (0, 0) is sprite (32, spr_top). */
#define SPR_X0     32
extern unsigned char spr_top;

/* the music: tune45's blob (WITCHGFX4) in bank 1; PLAY(TITLE) etc. */
#define MUSIC_LIN  0x001C000UL
#define PLAY(n)    tune_play_far(MUSIC_LIN + TUNE_##n##_AT)
