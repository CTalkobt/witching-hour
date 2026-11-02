/* video.c — the display
 *
 * The VIC-IV in 320 x 200 with 16-bit characters: those above $FF are full
 * colour (64 bytes, a byte a pixel: the palette entry), from GFX1_LIN on.
 * Nothing is changed while the screen is drawn: all of it is in the screen
 * RAM, which is in bank 0 (so the CPU writes it directly) and is changed in
 * the vertical blank.
 *
 * A row is RCELLS cells, read through the raster-rewrite buffer (RRB):
 *   cell 0       GOTOX 8 - fine: the row's fine scroll
 *   cells 1-41   the background (41: one more for the scroll)
 *   then SLOTS   objects: GOTOX x (transparent, a Y offset), two characters
 *   then 4       GOTOX 0, a black cell, GOTOX 312, a black cell: the
 *                plane's first 8 pixels (where the scroll starts) and its
 *                last 8 covered, so the play area is x 8-311 of the
 *                plane: 304 pixels, black (as the border) either side
 *
 * The rows are in bands (gfx.h): the HUD and the sky stand still; the far
 * hills, the near hills and the graveyard scroll each at its own speed. A
 * band's map is its rows, each its period of cells and the first 41 again,
 * so a window of 41 is always in one piece: video_scroll copies it by DMA
 * when the band moves a whole cell, and sets the rows' GOTOX.
 *
 * Objects are 16 x 16. Each 8-pixel column of one is a strip of 4
 * characters, one after another in memory: blank, the top half, the bottom
 * half, blank. The GOTOX's Y offset (8 bytes a step) shows a strip from any
 * pixel row, so an object at pixel row dy of a text row is drawn in that
 * row and the next one or two, each a slot: GOTOX, column A, column B
 * (column B's strip is 4 characters after column A's).
 */
#include <dma.h>
#include <audio_tune.h>
#include "hw.h"
#include "video.h"

#define PAL_R(i) REG(0xD100 + (i))
#define PAL_G(i) REG(0xD200 + (i))
#define PAL_B(i) REG(0xD300 + (i))

#define BOTTOM_LINE 252              /* the frame's interrupt: below the last row */
#define SLOTS   11                   /* objects a text row */
#define BASE    1                    /* the background's first cell */
#define OBJ0    (BASE + LCOLS)       /* the first object slot's cell */
#define EDGE    (OBJ0 + SLOTS * 3) /* GOTOX 0, black, GOTOX 312, black: the edges */
#define RCELLS  (EDGE + 4)
#define RBYTES  (RCELLS * 2)
#define EMPTY_X 360                  /* an unused slot draws there: right of the display */

unsigned char spr_top = 51;          /* the first text line (VIC-II lines) */
volatile unsigned char frames;
unsigned char bands_on, shake;
unsigned int scroll_speed = 0x100;

static unsigned int textx;           /* TEXTXPOS as the ROM set it */
static unsigned char saved[28], spr_saved76;

_Alignas(16) unsigned int spr_ptr[8];
/* the screen: bank 0 at $C000, where the C65's interface ROM shows
 * unless $D030 bit 5 (ROM at $C000) is clear - it is while the game runs */
#define screen ((unsigned char *)0xC000)
_Static_assert(ROWS * RBYTES <= 0x1000, "the screen must fit in $C000-$CFFF");
static unsigned char used[ROWS];     /* object slots used, this frame */
static unsigned char was_used[ROWS]; /* and the last */

extern unsigned int irq_old;         /* load.s45 */
extern void irq_entry(void);

/* ---- the frame's interrupt: the frame count and the music ---- */

__interrupt void raster_isr(void) {
    REG(0xD019) = 0x01;
    frames++;
    tune_update();
}

static void irq_on(void) {
    __asm__("sei");
    irq_old = REG(0x0314) | (unsigned int)REG(0x0315) << 8;
    saved[20] = REG(0xD01A);
    REG(0xDC0D) = 0x7F;                         /* the KERNAL's timer interrupt off */
    REG(0xD011) = REG(0xD011) & 0x7F;
    REG(0xD012) = BOTTOM_LINE;
    REG(0x0314) = (unsigned char)(unsigned int)irq_entry;
    REG(0x0315) = (unsigned char)((unsigned int)irq_entry >> 8);
    REG(0xD01A) = 0x01;
    REG(0xD019) = 0x0F;
    __asm__("cli");
}

static void irq_off(void) {
    __asm__("sei");
    REG(0xD01A) = saved[20] & 0x0F;
    REG(0xD019) = 0x0F;
    REG(0x0314) = (unsigned char)irq_old;
    REG(0x0315) = (unsigned char)(irq_old >> 8);
    REG(0xDC0D) = 0x81;                         /* the timer interrupt back */
    __asm__("cli");
}

void video_wait(void) {
    unsigned char f = frames;
    while (frames == f) {}
}

/* ---- hardware sprites (V400 Y: physical lines, each row doubled) ---- */

static const unsigned char bit[8] = {1, 2, 4, 8, 16, 32, 64, 128};

void spr_hide(unsigned char n) {
    REG(0xD015) &= (unsigned char)~bit[n];
}

void spr_set(unsigned char n, signed int x, signed int y, unsigned int img, unsigned char tall) {
    unsigned char b = bit[n];
    unsigned int px, py;
    if (x <= -16 || x >= SCREEN_W || y <= -96 || y >= 200) {
        spr_hide(n);
        return;
    }
    px = (unsigned int)(x + SPR_X0);
    py = (unsigned int)((y + spr_top) * 2);
    REG(0xD000 + n * 2) = (unsigned char)px;
    REG(0xD001 + n * 2) = (unsigned char)py;
    if (px & 0x100) REG(0xD010) |= b; else REG(0xD010) &= (unsigned char)~b;
    if (py & 0x100) REG(0xD077) |= b; else REG(0xD077) &= (unsigned char)~b;
    if (tall) REG(0xD055) |= b; else REG(0xD055) &= (unsigned char)~b;
    spr_ptr[n] = img;
    REG(0xD015) |= b;
}

/* ---- objects ---- */

void obj_begin(void) {
    unsigned char r;
    for (r = 0; r < ROWS; r++) {
        if (used[r] > was_used[r]) was_used[r] = used[r];
        used[r] = 0;
    }
}

static void put_char(unsigned char *p, unsigned int ch) {
    p[0] = (unsigned char)ch;
    p[1] = (unsigned char)(ch >> 8);
}

void obj_add(signed int x, signed int y, unsigned int obj) {
    obj_add_part(x, y, obj, 0, 16);
}

/* only the object's pixel rows top .. top + h - 1 are drawn (so a small
 * thing in the middle of its 16 x 16 takes the text rows it is on, not 3) */
void obj_add_part(signed int x, signed int y, unsigned int obj, unsigned char top, unsigned char h) {
    signed int row, last, s;
    unsigned int gx;
    unsigned char cols = 3;
    unsigned char *p;
    if (x <= -16 || x >= SCREEN_W || y <= -16 || y >= 200) return;
    if (x < -8) {                    /* only column B shows: draw it as column A */
        x += 8;
        obj += 4;
        cols = 1;
    }
    gx = (unsigned int)(x + 8);
    row = y + top < 0 ? 0 : ((y + top) >> 3);
    last = (y + top + h - 1) >> 3;
    if (last >= ROWS) last = ROWS - 1;
    for (; row <= last; row++) {
        if (used[row] == SLOTS) continue;
        s = 8 + row * 8 - y;         /* the strip's row at the text row's top */
        p = screen + (unsigned int)row * RBYTES + (OBJ0 + used[row]++ * 3) * 2;
        p[0] = (unsigned char)gx;
        p[1] = (unsigned char)((gx >> 8) & 3) | (unsigned char)((s & 7) << 5);
        put_char(p + 2, obj + (s >> 3));
        put_char(p + 4, cols & 2 ? obj + 4 + (s >> 3) : BLANK_CHAR);
    }
}

void obj_end(void) {
    unsigned char r, k;
    unsigned char *p;
    for (r = 0; r < ROWS; r++)
        for (k = used[r]; k < was_used[r]; k++) {
            p = screen + (unsigned int)r * RBYTES + (OBJ0 + k * 3) * 2;
            p[0] = (unsigned char)EMPTY_X;
            p[1] = (unsigned char)(EMPTY_X >> 8);
        }
    for (r = 0; r < ROWS; r++) was_used[r] = used[r];
}

/* ---- the screen ---- */

#define CELL(col, row) (screen + (unsigned int)(row) * RBYTES + ((col) + BASE) * 2)

void video_cell(unsigned char col, unsigned char row, unsigned int ch) {
    put_char(CELL(col, row), ch);
}

static void row_from(unsigned char row, unsigned long src) {
    dma_copy_far((unsigned long)(unsigned int)CELL(0, row), src, LCOLS * 2);
}

void video_picture(unsigned long at) {
    unsigned char r;
    for (r = 0; r < ROWS; r++) row_from(r, at + (unsigned int)r * LCOLS * 2);
}

void video_blank(unsigned char row) {
    row_from(row, SCR_GAME + (unsigned int)row * LCOLS * 2);
}

void video_clear_row(unsigned char row) {
    unsigned char c;
    for (c = 0; c < LCOLS; c++) video_cell(c, row, BLANK_CHAR);
}

/* cc45's strings are PETSCII: letters $41-$5A (lower case) or $C1-$DA */
static unsigned int glyph(unsigned char c, unsigned char row) {
    if (c >= 0xC1 && c <= 0xDA) c -= 0x80;
    if (c < 32 || c > 95) c = 32;
    if (row == 3) return FONT_SKY3 + (c - 32);
    if (row == 5) return FONT_SKY5 + (c - 32);
    return FONT_CHAR + (c - 32);
}

void video_text(unsigned char col, unsigned char row, const char *s) {
    while (*s && col < 38) video_cell(col++, row, glyph((unsigned char)*s++, row));
}

void video_center(unsigned char row, const char *s) {
    unsigned char n = 0;
    while (s[n]) n++;
    video_text((unsigned char)((38 - n) / 2), row, s);
}

void video_num(unsigned char col, unsigned char row, unsigned long v, unsigned char digits) {
    char b[11];
    unsigned char i = digits;
    b[i] = 0;
    while (i) {
        b[--i] = (char)('0' + (unsigned char)(v % 10));
        v /= 10;
    }
    video_text(col, row, b);
}

/* ---- the bands ---- */

static unsigned char band_frac[NBANDS];
static unsigned int band_px[NBANDS];    /* the scroll in pixels, 0 .. period * 8 - 1 */

static void band_copy(unsigned char b) {
    unsigned char r;
    unsigned long src = band_map[b] + (band_px[b] >> 3) * 2;
    for (r = 0; r < band_rows[b]; r++) {
        row_from(band_row[b] + r, src);
        src += band_stride[b];
    }
}

void video_bands_draw(void) {
    unsigned char b;
    for (b = 0; b < NBANDS; b++) band_copy(b);
}

static void set_fine(unsigned char row, unsigned char n, unsigned char x) {
    unsigned char *p = screen + (unsigned int)row * RBYTES;
    while (n--) {
        p[0] = x;
        p += RBYTES;
    }
}

void video_title_scroll(void) {
    set_fine(0, ROWS, 8);
}

void video_scroll(void) {
    unsigned char b;
    unsigned int old, x, step, period;
    signed char sh = 0;
    if (shake) {
        shake--;
        sh = (shake & 2) ? 2 : -2;
    }
    set_fine(0, band_row[0], (unsigned char)(8 + sh));
    for (b = 0; b < NBANDS; b++) {
        if (bands_on && band_speed[b]) {
            step = (unsigned int)(((unsigned long)band_speed[b] * scroll_speed) >> 8);
            old = band_px[b];
            x = band_frac[b] + (step & 0xFF);
            band_frac[b] = (unsigned char)x;
            period = band_period[b] * 8;
            band_px[b] += (step >> 8) + (x >> 8);
            if (band_px[b] >= period) band_px[b] -= period;
            if ((band_px[b] >> 3) != (old >> 3)) band_copy(b);
        }
        set_fine(band_row[b], band_rows[b], (unsigned char)(8 - (band_px[b] & 7) + sh));
    }
}

/* ---- palettes ---- */

static unsigned char fade = 16, flash_on;

static unsigned char nib(unsigned char v) {
    return (unsigned char)(v << 4 | v >> 4);
}

static unsigned char lev(unsigned char c) {
    if (flash_on) c = (unsigned char)((c + 255) >> 1);
    return nib((unsigned char)((c * fade) >> 4));
}

static void put_palettes(void) {
    unsigned int i;
    REG(0xD070) = 0x40 | 0x10 | 0x08 | 0x01;    /* edit bank 1: the screen's */
    for (i = 0; i < 256; i++) {
        PAL_R(i) = lev(pal_r[i]);
        PAL_G(i) = lev(pal_g[i]);
        PAL_B(i) = lev(pal_b[i]);
    }
    REG(0xD070) = 0x80 | 0x10 | 0x08 | 0x01;    /* bank 2: the sprites' */
    for (i = 0; i < 128; i++) {
        PAL_R(i) = lev(spal_r[i & 15]);
        PAL_G(i) = lev(spal_g[i & 15]);
        PAL_B(i) = lev(spal_b[i & 15]);
    }
    REG(0xD070) = 0x10 | 0x08 | 0x01;
}

void video_fade(unsigned char level) {
    fade = level;
    put_palettes();
}

void video_flash(unsigned char on) {
    flash_on = on;
    put_palettes();
}

void video_palette_cycle(void) {
    static unsigned char t;
    unsigned char i, e, k;
    t++;
    if (t & 3) return;
    REG(0xD070) = 0x40 | 0x10 | 0x08 | 0x01;
    for (i = 0; i < CYCLE_N; i++) {
        e = cycle_entry[i];
        k = cycle_src[i * 4 + ((t >> 2) + i * 3) % 4];
        PAL_R(e) = lev(pal_r[k]);
        PAL_G(e) = lev(pal_g[k]);
        PAL_B(e) = lev(pal_b[k]);
    }
    REG(0xD070) = 0x10 | 0x08 | 0x01;
}

/* ---- on and off ---- */

static unsigned char crow[RBYTES];

static void screen_init(void) {
    unsigned char r, k;
    unsigned char *p;
    for (r = 0; r < ROWS; r++) {
        p = screen + (unsigned int)r * RBYTES;
        p[0] = 8;
        p[1] = 0;
        for (k = 0; k < LCOLS; k++) put_char(p + (BASE + k) * 2, BLANK_CHAR);
        for (k = 0; k < SLOTS; k++) {
            put_char(p + (OBJ0 + k * 3) * 2, EMPTY_X);
            put_char(p + (OBJ0 + k * 3) * 2 + 2, BLANK_CHAR);
            put_char(p + (OBJ0 + k * 3) * 2 + 4, BLANK_CHAR);
        }
        put_char(p + EDGE * 2, 0);
        put_char(p + EDGE * 2 + 2, BLANK_CHAR);
        put_char(p + EDGE * 2 + 4, 312);
        put_char(p + EDGE * 2 + 6, BLANK_CHAR);
        used[r] = was_used[r] = 0;
    }
    /* the colour RAM: GOTOX cells (the objects' draw transparent), the rest 0 */
    for (k = 0; k < RBYTES; k++) crow[k] = 0;
    crow[0] = 0x10;
    crow[1] = 0xFF;
    for (k = 0; k < SLOTS; k++) {
        crow[(OBJ0 + k * 3) * 2] = 0x90;
        crow[(OBJ0 + k * 3) * 2 + 1] = 0xFF;
    }
    crow[EDGE * 2] = 0x10;
    crow[EDGE * 2 + 1] = 0xFF;
    crow[EDGE * 2 + 4] = 0x10;
    crow[EDGE * 2 + 5] = 0xFF;
    for (r = 0; r < ROWS; r++)
        dma_copy_far(COLOR_LIN + (unsigned int)r * RBYTES, (unsigned long)(unsigned int)crow, RBYTES);
}

void video_on(void) {
    unsigned char s;
    unsigned int scr = (unsigned int)screen;
    REG(0xD02F) = 0x47;                         /* the VIC-IV's registers */
    REG(0xD02F) = 0x53;
    saved[0] = REG(0xD031); saved[1] = REG(0xD054); saved[2] = REG(0xD058); saved[3] = REG(0xD059);
    saved[5] = REG(0xD05E); saved[6] = REG(0xD060); saved[7] = REG(0xD061);
    saved[8] = REG(0xD062); saved[9] = REG(0xD063); saved[10] = REG(0xD064); saved[11] = REG(0xD065);
    saved[12] = REG(0xD020); saved[13] = REG(0xD021); saved[14] = REG(0xD070); saved[15] = REG(0xD07B);
    saved[18] = REG(0xD06C); saved[19] = REG(0xD06D); saved[21] = REG(0xD06E); saved[22] = REG(0xD06B);
    saved[23] = REG(0xD057); spr_saved76 = REG(0xD076); saved[25] = REG(0xD017); saved[26] = REG(0xD055);
    saved[27] = REG(0xD056);
    textx = REG(0xD04C) | (unsigned int)(REG(0xD04D) & 0x0F) << 8;

    REG(0xD015) = 0;
    saved[4] = REG(0xD030);
    REG(0xD030) = saved[4] & 0xDF;              /* RAM at $C000-$CFFF: the screen */
    screen_init();
    REG(0xD020) = 0;
    REG(0xD021) = 0;
    saved[24] = REG(0xD05D);
    REG(0xD05D) = REG(0xD05D) & 0x7F;           /* hot registers off */
    REG(0xD031) = (REG(0xD031) & 0x77) | 0x40;  /* 320 x 200, FAST */
    REG(0xD054) = (REG(0xD054) & 0xEF) | 0x45;  /* CHR16 + FCLRHI, VFAST; sprites lo-res */
    REG(0xD058) = (unsigned char)RBYTES;
    REG(0xD059) = (unsigned char)(RBYTES >> 8);
    REG(0xD05E) = (unsigned char)RCELLS;
    REG(0xD063) = (REG(0xD063) & 0xC0) | (unsigned char)((RCELLS >> 8) << 4);
    REG(0xD07B) = ROWS;
    REG(0xD060) = (unsigned char)scr;
    REG(0xD061) = (unsigned char)(scr >> 8);
    REG(0xD062) = 0;
    REG(0xD064) = 0;
    REG(0xD065) = 0;
    /* the character plane 8 pixels (16 half pixels) left, under the border */
    spr_top = (unsigned char)((REG(0xD04E) | (unsigned int)(REG(0xD04F) & 0x0F) << 8) >> 1);
    put_palettes();

    /* sprites: 16 colours, 16 pixels, 8 bytes a row, 16-bit pointers; Y in
     * physical lines, each row doubled */
    for (s = 0; s < 8; s++) REG(0xD027 + s) = 0;
    REG(0xD06C) = (unsigned char)(unsigned int)spr_ptr;
    REG(0xD06D) = (unsigned char)((unsigned int)spr_ptr >> 8);
    REG(0xD06E) = 0x80;
    REG(0xD06B) = 0xFF;
    REG(0xD057) = 0xFF;
    REG(0xD055) = 0;
    REG(0xD056) = 48;
    REG(0xD076) = 0xFF;
    REG(0xD017) = 0xFF;
    REG(0xD077) = 0;
    REG(0xD078) = 0;
    REG(0xD01B) = 0;
    REG(0xD01D) = 0;
    REG(0xD05F) = 0;
    irq_on();
}

/* around a KERNAL call (saving): the ROM at $C000 again, the KERNAL's
 * interrupt; the screen stays as it is */
void video_kernal(unsigned char enter) {
    if (enter) {
        irq_off();
        REG(0xD030) = saved[4];
    } else {
        REG(0xD030) = saved[4] & 0xDF;
        irq_on();
    }
}

void video_off(void) {
    irq_off();
    REG(0xD015) = 0;
    REG(0xD06C) = saved[18]; REG(0xD06D) = saved[19]; REG(0xD06E) = saved[21]; REG(0xD06B) = saved[22];
    REG(0xD057) = saved[23]; REG(0xD076) = spr_saved76; REG(0xD017) = saved[25]; REG(0xD055) = saved[26];
    REG(0xD056) = saved[27];

    REG(0xD070) = saved[14];
    REG(0xD054) = saved[1];
    REG(0xD058) = saved[2]; REG(0xD059) = saved[3];
    REG(0xD05E) = saved[5];
    REG(0xD060) = saved[6]; REG(0xD061) = saved[7]; REG(0xD062) = saved[8]; REG(0xD063) = saved[9];
    REG(0xD064) = saved[10]; REG(0xD065) = saved[11];
    REG(0xD07B) = saved[15];
    REG(0xD05D) = saved[24];
    REG(0xD031) = saved[0];
    REG(0xD020) = saved[12]; REG(0xD021) = saved[13];
    REG(0xD030) = saved[4];
}
