/* game.h — the game: a night's flight, and what the front end needs */
#pragma once

/* controls (ctl_read) */
#define C_UP    0x01
#define C_DOWN  0x02
#define C_LEFT  0x04
#define C_RIGHT 0x08
#define C_FIRE  0x10
#define C_BOMB  0x20
#define C_PAUSE 0x40
#define C_QUIT  0x80
unsigned char ctl_read(void);

/* a game from night 1; demo: the computer flies (the attract mode).
 * Returns when the game is over (or, demo, after a while / a key). */
void game_play(unsigned char demo);
extern unsigned long score;
extern unsigned char night_reached;

/* random numbers; a sine (0-255 a turn, -64..64) */
unsigned char rnd(void);
signed char sine8(unsigned char a);

/* the front end (main.c) */
void title_draw_hud(void);
