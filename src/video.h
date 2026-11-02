/* video.h — the display: 320 x 200 full-colour characters in parallax
 * bands, objects drawn over them by the raster-rewrite buffer, and the
 * hardware sprites
 */
#pragma once
#include "gfx.h"

unsigned char gfx_load(void);              /* load.s45: WITCHGFX1-3 from disk */

void video_on(void);
void video_off(void);
void video_kernal(unsigned char enter);    /* 1 before a KERNAL call (disk), 0 after */
void video_wait(void);                     /* until the next frame (the bottom border) */
extern volatile unsigned char frames;      /* counted by the interrupt */

/* the screen: a whole screen picture (gfx.h SCR_), or cells */
void video_picture(unsigned long at);
void video_cell(unsigned char col, unsigned char row, unsigned int ch);
/* text in the game's font (upper case, digits, punctuation) */
void video_text(unsigned char col, unsigned char row, const char *s);
void video_center(unsigned char row, const char *s);
void video_num(unsigned char col, unsigned char row, unsigned long v, unsigned char digits);
void video_blank(unsigned char row);       /* the game picture's row again (under text) */
void video_clear_row(unsigned char row);   /* a row of black */

/* the parallax bands: scrolling at speed (8.8 px a frame) times their own */
extern unsigned char bands_on;
extern unsigned int scroll_speed;
void video_scroll(void);                   /* once a frame, just after video_wait */
void video_title_scroll(void);             /* the same for a still picture (the title) */
void video_bands_draw(void);               /* every band's rows at their place */

/* the palettes: fade (0 black - 16 as drawn), a flash (lightning) */
void video_fade(unsigned char level);
void video_flash(unsigned char on);
void video_palette_cycle(void);            /* once a frame: stars twinkle, lights flicker */

/* objects (16 x 16, gfx.h OBJ_): obj_begin, obj_add for each, obj_end; in
 * the vertical blank, just after video_wait. x, y: the top left in screen
 * pixels. */
void obj_begin(void);
void obj_add(signed int x, signed int y, unsigned int obj);
void obj_add_part(signed int x, signed int y, unsigned int obj, unsigned char top, unsigned char h);
void obj_end(void);

/* hardware sprites 0-7 (gfx.h IMG_): 16 colours, 16 x 21 or 16 x 48 (tall) */
void spr_set(unsigned char n, signed int x, signed int y, unsigned int img, unsigned char tall);
void spr_hide(unsigned char n);

/* the screen shakes (an explosion): frames */
extern unsigned char shake;
