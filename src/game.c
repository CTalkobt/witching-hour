/* game.c — a night's flight
 *
 * The witch flies right over the graveyard at midnight. Bats come in
 * flocks, ghosts drift after her, jack-o'-lanterns bounce along spitting
 * fire, flaming skulls stop, stare and charge. Her spell (FIRE, held for
 * a stream) gets wider with each potion; candy is points, a black cat a
 * life. The midnight toll (B, or the second button) strikes everything on
 * the screen, three a night. Kills in quick succession build a chain, and
 * the chain multiplies their points (up to five times). At the night's end
 * comes its master - the Pumpkin King, the Bone Lord or the Vampire Count,
 * in turn - to Bach's Toccata or, for the Count, the Dies irae.
 *
 * The music is In the Hall of the Mountain King, and as in Grieg it gets
 * faster and faster: its tempo follows the night's progress.
 *
 * Positions are in 16ths of a pixel (signed). Everything but the witch and
 * the bosses is an RRB object (video.c); they are hardware sprites 0-1 and
 * 2-4.
 */
#include <input.h>
#include <mega65.h>
#include <audio_tune.h>
#include "hw.h"
#include "video.h"
#include "music.h"
#include "sfx.h"
#include "game.h"

#define FP(x) ((signed int)(x) * 16)
#define PX(v) ((signed int)(v) >> 4)

/* the play area for things (pixels) */
#define TOP     10
#define BOTTOM  180
#define FLOOR   168                  /* where pumpkins bounce */

#define NE 14                        /* enemies */
#define NS 12                        /* the witch's spells */
#define NB 14                        /* enemy bullets */
#define NP 4                         /* pickups */
#define NX 10                        /* explosions */

#define NIGHT_FRAMES 4200            /* 84 seconds of waves before the boss */

enum { E_NONE, E_BAT, E_GHOST, E_PUMP, E_SKULL };
enum { B_FIRE, B_TOXIC, B_SEED };
enum { P_CANDY, P_POTION, P_CAT };

unsigned long score, hiscore_seen;
unsigned char night_reached;

static unsigned int demo_t;
static unsigned char demo, night, lives, power, bombs, quit;
static signed int wx, wy;            /* the witch (pixels, top left of 32 x 21) */
static unsigned char invuln, dead_t, fire_t, bomb_t, wanim;
static unsigned int prog;            /* frames into the night */
static unsigned char wave_t, wave_n, frame;
static unsigned int lightning_t;
static unsigned char flash_t;
static unsigned long next_life;
static unsigned char chain, chain_t, mult, chain_msg;   /* the kill chain */

static unsigned char e_type[NE], e_hp[NE], e_t[NE], e_ph[NE], e_flash[NE];
static signed int e_x[NE], e_y[NE], e_vx[NE], e_vy[NE], e_base[NE];
static unsigned char s_on[NS];
static signed int s_x[NS], s_y[NS], s_vy[NS];
static unsigned char b_on[NB], b_kind[NB];
static signed int b_x[NB], b_y[NB], b_vx[NB], b_vy[NB];
static unsigned char p_on[NP], p_kind[NP], p_t[NP];
static signed int p_x[NP], p_y[NP];
static unsigned char x_on[NX], x_f[NX], x_kind[NX];
static signed int x_x[NX], x_y[NX];

/* the boss */
static unsigned int boss_hp, boss_max;
static unsigned char boss_on, boss_kind, boss_t, boss_flash, boss_mode, boss_dying, boss_hide;
static signed int bx, by, boss_ty;

static const signed char sine[64] = {
    0, 6, 12, 19, 24, 30, 36, 41, 45, 49, 53, 56, 59, 61, 63, 64,
    64, 64, 63, 61, 59, 56, 53, 49, 45, 41, 36, 30, 24, 19, 12, 6,
    0, -6, -12, -19, -24, -30, -36, -41, -45, -49, -53, -56, -59, -61, -63, -64,
    -64, -64, -63, -61, -59, -56, -53, -49, -45, -41, -36, -30, -24, -19, -12, -6,
};

/* ---------------------------------------------------------------- odds and ends */

static unsigned int seed = 0x1D2B;

unsigned char rnd(void) {
    seed ^= seed << 7;
    seed ^= seed >> 9;
    seed ^= seed << 8;
    return (unsigned char)seed;
}

#define CRSR_LEFTUP REG(0xD60F)

unsigned char ctl_read(void) {
    unsigned char c = 0, joy = input_joy(1) | input_joy(2), lu = CRSR_LEFTUP & 3;
    if (input_pressed(KEY_STOP)) c |= C_QUIT;
    if (input_pressed(KEY_W) || (lu & 2) || (joy & JOY_UP)) c |= C_UP;
    if (input_pressed(KEY_S) || (input_pressed(KEY_DOWN) && !(lu & 2)) || (joy & JOY_DOWN)) c |= C_DOWN;
    if (input_pressed(KEY_A) || (lu & 1) || (joy & JOY_LEFT)) c |= C_LEFT;
    if (input_pressed(KEY_D) || (input_pressed(KEY_RIGHT) && !(lu & 1)) || (joy & JOY_RIGHT)) c |= C_RIGHT;
    if (input_pressed(KEY_SPACE) || input_pressed(KEY_RETURN) || (joy & JOY_FIRE)) c |= C_FIRE;
    if (input_pressed(KEY_B) || input_pressed(KEY_M) || (joy & JOY_FIRE2)) c |= C_BOMB;
    if (input_pressed(KEY_P)) c |= C_PAUSE;
    return c;
}

static unsigned char hit(signed int ax, signed int ay, unsigned char aw, unsigned char ah,
                         signed int bx2, signed int by2, unsigned char bw, unsigned char bh) {
    return ax < bx2 + bw && bx2 < ax + aw && ay < by2 + bh && by2 < ay + ah;
}

/* ---------------------------------------------------------------- the HUD */

static unsigned long shown_score = 0xFFFFFFFFUL;
static unsigned char shown_lives = 0xFF, shown_bombs = 0xFF, shown_boss = 0xFF, shown_mult = 0xFF;
extern unsigned long hiscore;        /* main.c: the table's best */

static void hud(unsigned char all) {
    unsigned char i;
    if (all) {
        video_clear_row(0);
        shown_score = 0xFFFFFFFFUL;
        shown_lives = shown_bombs = shown_boss = shown_mult = 0xFF;
        video_text(29, 0, "HI");
    }
    if (score != shown_score) {
        shown_score = score;
        video_num(0, 0, score, 7);
        if (score > hiscore) video_num(31, 0, score, 7);
        else if (all) video_num(31, 0, hiscore, 7);
    }
    if (lives != shown_lives) {
        shown_lives = lives;
        for (i = 0; i < 5; i++) video_text(8 + i, 0, i + 1 < lives ? "]" : " ");
    }
    if (bombs != shown_bombs) {
        shown_bombs = bombs;
        for (i = 0; i < 3; i++) video_text(14 + i, 0, i < bombs ? "[" : " ");
    }
    if (mult != shown_mult) {
        shown_mult = mult;
        if (mult > 1) {
            video_text(26, 0, "X");
            video_num(27, 0, mult, 1);
        } else
            video_text(26, 0, "  ");
    }
    if (boss_on) {
        unsigned char k = (unsigned char)((unsigned long)boss_hp * 7 / boss_max);
        if (k != shown_boss) {
            shown_boss = k;
            for (i = 0; i < 7; i++) video_text(18 + i, 0, i < k ? "=" : (i == 0 ? " " : "-"));
        }
    } else if (shown_boss != 0xFE) {
        shown_boss = 0xFE;
        video_text(18, 0, "NIGHT  ");
        video_num(24, 0, night, 1);
    }
}

static void add_score(unsigned int n) {
    score += n;
    if (score >= next_life) {
        next_life += 25000;
        if (lives < 6) {
            lives++;
            sfx_life();
        }
    }
}

/* ---------------------------------------------------------------- spawning */

static signed char free_slot(unsigned char *on, unsigned char n) {
    unsigned char i;
    for (i = 0; i < n; i++)
        if (!on[i]) return (signed char)i;
    return -1;
}

static signed char new_enemy(unsigned char type, signed int x, signed int y) {
    signed char i = free_slot(e_type, NE);
    if (i < 0) return -1;
    e_type[i] = type;
    e_x[i] = FP(x);
    e_y[i] = FP(y);
    e_base[i] = FP(y);
    e_vx[i] = 0;
    e_vy[i] = 0;
    e_t[i] = 0;
    e_ph[i] = rnd();
    e_flash[i] = 0;
    e_hp[i] = type == E_BAT ? 1 : type == E_GHOST ? 3 : type == E_PUMP ? 4 : 2;
    if (night > 2 && type != E_BAT) e_hp[i]++;
    return i;
}

static void enemy_bullet(signed int x, signed int y, unsigned char kind, signed int vx, signed int vy) {
    signed char i = free_slot(b_on, NB);
    if (i < 0) return;
    b_on[i] = 1;
    b_kind[i] = kind;
    b_x[i] = x;
    b_y[i] = y;
    b_vx[i] = vx;
    b_vy[i] = vy;
}

/* a bullet from (x, y) (16ths) at the witch, speed sp (16ths a frame) */
static void aimed(signed int x, signed int y, unsigned char kind, unsigned char sp) {
    signed int dx = FP(wx + 12) - x, dy = FP(wy + 6) - y, ax, ay, len;
    ax = dx < 0 ? -dx : dx;
    ay = dy < 0 ? -dy : dy;
    len = (ax > ay ? ax + ay / 2 : ay + ax / 2) / 16;
    if (len < 1) len = 1;
    enemy_bullet(x, y, kind, (signed int)((signed long)dx * sp / 16 / len),
                 (signed int)((signed long)dy * sp / 16 / len));
    sfx_enemy_shot();
}

static void explode(signed int x, signed int y, unsigned char kind) {
    signed char i = free_slot(x_on, NX);
    if (i < 0) return;
    x_on[i] = 1;
    x_f[i] = 0;
    x_kind[i] = kind;
    x_x[i] = x;
    x_y[i] = y;
}

static void drop(signed int x, signed int y, unsigned char kind) {
    signed char i = free_slot(p_on, NP);
    if (i < 0) return;
    p_on[i] = 1;
    p_kind[i] = kind;
    p_x[i] = x;
    p_y[i] = y;
    p_t[i] = 0;
}

/* the waves: a flock, ghosts, pumpkins, skulls, mixtures */
static void wave(void) {
    unsigned char k, kind, n;
    signed int y;
    kind = (unsigned char)(wave_n % 8);
    if (wave_n > 8 && (rnd() & 3) == 0) kind = rnd() & 7;
    wave_n++;
    switch (kind) {
    case 0: case 4:                  /* a line of bats on a wave */
        y = 30 + (rnd() & 63) + (rnd() & 31);
        n = 4 + (night > 1) + (night > 3);
        for (k = 0; k < n; k++) {
            signed char i = new_enemy(E_BAT, SCREEN_W + k * 22, y);
            if (i >= 0) e_ph[i] = (unsigned char)(k * 10);
        }
        sfx_screech();
        break;
    case 1:                          /* ghosts */
        n = 2 + (night > 2);
        for (k = 0; k < n; k++) new_enemy(E_GHOST, SCREEN_W + k * 40, 20 + (rnd() % 140));
        break;
    case 2: case 6:                  /* pumpkins bouncing */
        n = 2 + (night > 1);
        for (k = 0; k < n; k++) {
            signed char i = new_enemy(E_PUMP, SCREEN_W + k * 50, 100 + (rnd() & 31));
            if (i >= 0) e_vy[i] = -FP(2);
        }
        break;
    case 3:                          /* skulls */
        n = 2 + (night > 2);
        for (k = 0; k < n; k++) new_enemy(E_SKULL, SCREEN_W + k * 12, 24 + (rnd() % 130));
        break;
    case 5:                          /* a V of bats */
        y = 60 + (rnd() & 63);
        for (k = 0; k < 5; k++) {
            signed char i = new_enemy(E_BAT, SCREEN_W + (k < 3 ? k : 4 - k) * 18 + (k > 2) * 6, y - 40 + k * 20);
            if (i >= 0) e_ph[i] = 0;
        }
        sfx_screech();
        break;
    case 7:                          /* everything */
        new_enemy(E_SKULL, SCREEN_W, 30 + (rnd() & 63));
        new_enemy(E_GHOST, SCREEN_W + 30, 90 + (rnd() & 63));
        for (k = 0; k < 3; k++) new_enemy(E_BAT, SCREEN_W + 60 + k * 20, 50 + k * 30);
        break;
    }
}

/* ---------------------------------------------------------------- the witch */

static void witch_shoot(void) {
    signed char i;
    signed char k, n = power >= 3 ? 3 : (signed char)power;
    for (k = 0; k < n; k++) {
        i = free_slot(s_on, NS);
        if (i < 0) return;
        s_on[i] = 1;
        s_x[i] = FP(wx + 22);
        s_y[i] = FP(wy + 3);
        s_vy[i] = 0;
        if (n == 2) s_y[i] += k ? FP(5) : -FP(4);
        if (n == 3 && k) s_vy[i] = k == 1 ? -12 : 12;
    }
    sfx_shot();
}

static void witch_die(void) {
    unsigned char k;
    for (k = 0; k < 4; k++) explode(wx + (rnd() & 15), wy - 4 + (rnd() & 15), k & 1);
    sfx_boom();
    sfx_dead();
    shake = 20;
    dead_t = 90;
    chain = 0;
    chain_t = 0;
    mult = 1;
    if (power > 1) power--;
    lives--;
}

static void ai(unsigned char *c) {
    /* the attract mode: after the nearest enemy in front, out of bullets' way */
    unsigned char i;
    signed int ty = 90, best = 999, d;
    for (i = 0; i < NE; i++)
        if (e_type[i] && PX(e_x[i]) > wx + 20) {
            d = PX(e_x[i]) - wx;
            if (d < best) {
                best = d;
                ty = PX(e_y[i]) - 4;
            }
        }
    if (boss_on) ty = PX(by) + 14;
    for (i = 0; i < NB; i++)
        if (b_on[i]) {
            d = PX(b_x[i]) - wx;
            if (d > -8 && d < 70) {
                signed int dy = PX(b_y[i]) - (wy + 6);
                if (dy > -16 && dy < 16) ty = dy > 0 ? wy - 30 : wy + 30;
            }
        }
    *c = C_FIRE;
    if (ty < wy - 2) *c |= C_UP;
    if (ty > wy + 2) *c |= C_DOWN;
    if (wx < 40) *c |= C_RIGHT;
    if (wx > 70) *c |= C_LEFT;
    /* the toll when crowded */
    for (i = 0, d = 0; i < NB; i++) d += b_on[i];
    if (bombs && d > 6 && (rnd() & 15) == 0) *c |= C_BOMB;
}

static void bomb(void) {
    unsigned char i;
    bombs--;
    sfx_bomb();
    flash_t = 6;
    shake = 16;
    bomb_t = 30;
    for (i = 0; i < NB; i++) b_on[i] = 0;
    for (i = 0; i < NE; i++)
        if (e_type[i]) {
            explode(PX(e_x[i]), PX(e_y[i]), e_type[i] == E_PUMP);
            add_score(50);
            e_type[i] = E_NONE;
        }
    if (boss_on && !boss_dying && !boss_hide) {
        boss_hp = boss_hp > 40 ? boss_hp - 40 : 1;
        boss_flash = 10;
    }
}

static unsigned char last_ctl;

static void witch(void) {
    unsigned char c;
    signed int sp = 3;
    if (demo) {
        ai(&c);
        if (ctl_read() & (C_FIRE | C_QUIT)) quit = 1;
    } else {
        c = ctl_read();
        if (c & C_QUIT) quit = 1;
    }
    if (dead_t) {
        if (--dead_t == 0) {
            if (!lives) return;
            wx = 20;
            wy = 90;
            invuln = 120;
        }
        return;
    }
    if (c & C_UP) wy -= sp;
    if (c & C_DOWN) wy += sp;
    if (c & C_LEFT) wx -= sp;
    if (c & C_RIGHT) wx += sp;
    if (wx < 0) wx = 0;
    if (wx > SCREEN_W - 40) wx = SCREEN_W - 40;
    if (wy < TOP) wy = TOP;
    if (wy > BOTTOM - 4) wy = BOTTOM - 4;
    if (fire_t) fire_t--;
    if ((c & C_FIRE) && !fire_t) {
        witch_shoot();
        fire_t = 7;
    }
    if ((c & C_BOMB) && !(last_ctl & C_BOMB) && bombs && !bomb_t) bomb();
    if (bomb_t) bomb_t--;
    last_ctl = c;
    if (invuln) invuln--;
}

/* ---------------------------------------------------------------- the enemies */

static void kill_enemy(unsigned char i) {
    unsigned char t = e_type[i], r = rnd();
    signed int x = PX(e_x[i]), y = PX(e_y[i]);
    static const unsigned int pts[5] = {0, 100, 200, 300, 250};
    static const unsigned char chain_mult[6] = {4, 10, 18, 30, 255, 255};
    unsigned char m = 1;
    /* the chain: another kill within two seconds; 4, 10, 18 and 30 kills
     * multiply the points by 2, 3, 4 and 5 */
    if (chain < 250) chain++;
    chain_t = 100;
    while (chain >= chain_mult[m - 1]) m++;
    if (m > mult) {
        mult = m;
        chain_msg = 50;
        sfx_power();
    }
    add_score(pts[t] * mult);
    explode(x, y, t == E_PUMP || t == E_SKULL);
    if (t == E_PUMP || t == E_SKULL) sfx_crunch(); else sfx_pop();
    e_type[i] = E_NONE;
    if (r < 12) drop(x, y, P_CAT);
    else if (r < 50 && power < 3 && t != E_BAT) drop(x, y, P_POTION);
    else if (r < 110) drop(x, y, P_CANDY);
}

static void enemies(void) {
    unsigned char i, k;
    signed int x, y;
    unsigned char speed = 16 + night * 2 + (unsigned char)(prog >> 9);   /* 16ths a frame */
    for (i = 0; i < NE; i++) {
        if (!e_type[i]) continue;
        e_t[i]++;
        if (e_flash[i]) e_flash[i]--;
        switch (e_type[i]) {
        case E_BAT:
            e_x[i] -= speed + 6;
            e_ph[i] += 3;
            e_y[i] = e_base[i] + sine[e_ph[i] & 63] * 5 / 8 * 16 / 4;
            if (night > 1 && e_t[i] == 60 && (rnd() & 3) == 0) e_base[i] += wy > PX(e_y[i]) ? FP(30) : -FP(30);
            break;
        case E_GHOST:
            /* drifts in, then after the witch, slowly; now and then it fades */
            if (e_vx[i] > -20) e_vx[i]--;
            y = FP(wy) - e_y[i];
            if (y > 0 && e_vy[i] < 10) e_vy[i]++;
            if (y < 0 && e_vy[i] > -10) e_vy[i]--;
            e_x[i] += e_vx[i];
            e_y[i] += e_vy[i];
            if (e_x[i] < FP(wx) + FP(40) && e_vx[i] < -8 && e_t[i] > 100) e_vx[i] = -30;
            break;
        case E_PUMP:
            e_x[i] -= speed - 4;
            e_vy[i] += 3;
            e_y[i] += e_vy[i];
            if (e_y[i] > FP(FLOOR)) {
                e_y[i] = FP(FLOOR);
                e_vy[i] = -FP(3) - (rnd() & 15);
            }
            if ((unsigned char)(e_t[i] + i * 20) % (unsigned char)(110 - night * 8) == 0 && PX(e_x[i]) < SCREEN_W - 10 && PX(e_x[i]) > wx + 40)
                aimed(e_x[i] + FP(6), e_y[i] + FP(6), B_FIRE, (unsigned char)(28 + night * 2));
            break;
        case E_SKULL:
            if (e_t[i] < 30) e_x[i] -= FP(2);           /* in */
            else if (e_t[i] == 60) {                   /* a stare, a shot, then the charge */
                aimed(e_x[i], e_y[i] + FP(6), B_TOXIC, 34);
            } else if (e_t[i] == 80) {
                x = FP(wx + 10) - e_x[i];
                y = FP(wy) - e_y[i];
                e_vx[i] = -FP(4) - night * 4;
                e_vy[i] = (signed int)((signed long)y * 64 / (x < -FP(20) ? -x : FP(20)));
                sfx_charge();
            } else if (e_t[i] > 80) {
                e_x[i] += e_vx[i];
                e_y[i] += e_vy[i];
            }
            break;
        }
        x = PX(e_x[i]);
        y = PX(e_y[i]);
        if (x < -20 || y < -24 || y > 210) {
            e_type[i] = E_NONE;
            continue;
        }
        /* the witch's spells */
        for (k = 0; k < NS; k++)
            if (s_on[k] && hit(PX(s_x[k]) + 4, PX(s_y[k]) + 5, 10, 6, x + 2, y + 2, 12, 12)) {
                s_on[k] = 0;
                if (e_type[i] == E_GHOST && (e_t[i] & 64) && (e_t[i] & 32)) continue;   /* faded: through it */
                if (--e_hp[i] == 0) {
                    kill_enemy(i);
                    break;
                }
                e_flash[i] = 4;
                sfx_hit();
            }
        if (!e_type[i]) continue;
        /* the witch */
        if (!dead_t && !invuln && hit(wx + 10, wy + 8, 10, 8, x + 3, y + 3, 10, 10)) {
            kill_enemy(i);
            witch_die();
        }
    }
}

/* ---------------------------------------------------------------- the boss */

static void boss_start(void) {
    boss_on = 1;
    boss_kind = (unsigned char)((night - 1) % 3);   /* the King, the Lord, the Count */
    boss_hide = 0;
    boss_max = boss_hp = 260 + (unsigned int)night * 60;
    bx = FP(SCREEN_W + 10);
    by = FP(60);
    boss_ty = 60;
    boss_t = 0;
    boss_mode = 0;
    boss_dying = 0;
    boss_flash = 0;
    tune_tempo(0x100);
    if (boss_kind == 2) PLAY(COUNT);
    else PLAY(BOSS);
    video_center(3, boss_kind == 2 ? "THE VAMPIRE COUNT!" : boss_kind ? "THE BONE LORD RISES" : "THE PUMPKIN KING!");
    sfx_cackle();
}

static void boss(void) {
    unsigned char k;
    signed int x, y;
    if (!boss_on) return;
    boss_t++;
    if (boss_flash) boss_flash--;
    x = PX(bx);
    y = PX(by);
    if (boss_dying) {
        if ((boss_t & 7) == 0) {
            explode(x + (rnd() % 40), y + (rnd() % 40), rnd() & 1);
            sfx_crunch();
        }
        if ((boss_t & 15) == 0) sfx_boom();
        by += 6;
        boss_hide = 0;
        if (--boss_dying == 0) {
            boss_on = 0;
            shake = 30;
        }
        return;
    }
    if (boss_t == 60) video_blank(3);
    /* in from the right, then up and down, and in at the witch now and then */
    if (bx > FP(SCREEN_W - 70)) bx -= FP(1);
    else {
        if ((boss_t & 63) == 0) boss_ty = 20 + (rnd() % 120);
        if (PX(by) < boss_ty) by += 12 + night * 2;
        if (PX(by) > boss_ty) by -= 12 + night * 2;
    }
    /* attacks, in turns */
    if (bx <= FP(SCREEN_W - 70)) {
        k = boss_t & 127;
        if (boss_kind == 2) switch (boss_mode) {
        case 0:                      /* a ring of blood, turning */
            if (k == 20 || k == 60 || (night > 3 && k == 100))
                for (unsigned char a = 0; a < 64; a += 8)
                    enemy_bullet(FP(x + 18), FP(y + 20), B_FIRE,
                                 sine[(a + (boss_t >> 1) + 16) & 63] / 2, sine[(a + (boss_t >> 1)) & 63] / 2);
            break;
        case 1:                      /* away as a flock of bats, back elsewhere */
            if (k == 10) {
                boss_hide = 1;
                for (unsigned char n = 0; n < 4; n++) {
                    signed char i = new_enemy(E_BAT, x + 8 + n * 6, y + 4 + n * 12);
                    if (i >= 0) e_ph[i] = (unsigned char)(n * 16);
                }
                explode(x + 16, y + 16, 0);
                sfx_screech();
            }
            if (k == 90) {
                boss_ty = 20 + (rnd() % 120);
                by = FP(boss_ty);
                boss_hide = 0;
                explode(x + 16, PX(by) + 16, 0);
                sfx_cackle();
            }
            break;
        case 2:                      /* aimed, in pairs */
            if ((k & 31) == 0 || (k & 31) == 6) aimed(FP(x + 18), FP(y + 18), B_FIRE, (unsigned char)(32 + night * 2));
            break;
        }
        else switch (boss_mode) {
        case 0:                      /* a fan */
            if (k == 20 || k == 60 || (night > 2 && k == 100))
                for (signed char a = -2; a <= 2; a++)
                    enemy_bullet(FP(x + 8), FP(y + 30), boss_kind ? B_TOXIC : B_SEED, -FP(2), a * 12);
            break;
        case 1:                      /* aimed */
            if ((k & 15) == 0) aimed(FP(x + 8), FP(y + 30), B_FIRE, (unsigned char)(34 + night * 2));
            break;
        case 2:                      /* friends */
            if (k == 30) {
                for (unsigned char n = 0; n < 3; n++)
                    new_enemy(boss_kind ? E_SKULL : E_BAT, SCREEN_W + n * 20, 20 + n * 50 + (rnd() & 15));
                sfx_screech();
            }
            break;
        }
        if (k == 127) boss_mode = (unsigned char)((boss_mode + 1) % 3);
    }
    if (boss_hide) return;
    /* hits */
    for (k = 0; k < NS; k++)
        if (s_on[k] && hit(PX(s_x[k]) + 4, PX(s_y[k]) + 5, 10, 6, x + 6, y + 8, 36, 36)) {
            s_on[k] = 0;
            add_score(5);
            sfx_boss_hit();
            boss_flash = 3;
            if (--boss_hp == 0) {
                boss_dying = 120;
                add_score(5000 * (unsigned int)(night > 5 ? 5 : night));
                sfx_cackle();
                for (k = 0; k < NB; k++) b_on[k] = 0;
                break;
            }
        }
    if (!dead_t && !invuln && hit(wx + 10, wy + 8, 10, 8, x + 8, y + 8, 32, 34)) witch_die();
}

/* ---------------------------------------------------------------- the rest */

static void movers(void) {
    unsigned char i;
    signed int x, y;
    for (i = 0; i < NS; i++)
        if (s_on[i]) {
            s_x[i] += FP(7);
            s_y[i] += s_vy[i];
            if (PX(s_x[i]) > SCREEN_W || PX(s_y[i]) < 0 || PX(s_y[i]) > 196) s_on[i] = 0;
        }
    for (i = 0; i < NB; i++)
        if (b_on[i]) {
            b_x[i] += b_vx[i];
            b_y[i] += b_vy[i];
            x = PX(b_x[i]);
            y = PX(b_y[i]);
            if (x < -16 || x > SCREEN_W || y < 0 || y > 196) b_on[i] = 0;
            else if (!dead_t && !invuln && hit(wx + 10, wy + 8, 10, 8, x + 5, y + 5, 6, 6)) {
                b_on[i] = 0;
                witch_die();
            }
        }
    for (i = 0; i < NP; i++)
        if (p_on[i]) {
            p_t[i]++;
            p_x[i] -= (p_t[i] & 1);
            p_y[i] += sine[(p_t[i] * 2) & 63] > 0 ? (p_t[i] & 3) == 0 : -((p_t[i] & 3) == 0);
            if (p_y[i] < TOP) p_y[i] = TOP;
            if (p_y[i] > BOTTOM) p_y[i] = BOTTOM;
            if (p_x[i] < -16) p_on[i] = 0;
            else if (!dead_t && hit(wx + 4, wy + 2, 24, 18, p_x[i] + 2, p_y[i] + 2, 12, 12)) {
                p_on[i] = 0;
                if (p_kind[i] == P_CANDY) {
                    add_score(500);
                    sfx_pickup();
                } else if (p_kind[i] == P_POTION) {
                    if (power < 3) power++;
                    add_score(1000);
                    sfx_power();
                } else {
                    if (lives < 6) lives++;
                    sfx_life();
                    sfx_cackle();
                }
            }
        }
    for (i = 0; i < NX; i++)
        if (x_on[i] && ++x_f[i] >= 16) x_on[i] = 0;
}

/* ---------------------------------------------------------------- drawing */

static const unsigned int bat_obj[4] = {OBJ_BAT_0, OBJ_BAT_1, OBJ_BAT_2, OBJ_BAT_1};
static const unsigned int burst_obj[4] = {OBJ_BURST_0, OBJ_BURST_1, OBJ_BURST_2, OBJ_BURST_3};
static const unsigned int fburst_obj[4] = {OBJ_FBURST_0, OBJ_FBURST_1, OBJ_FBURST_2, OBJ_FBURST_3};
static const unsigned int bullet_obj[3][2] = {{OBJ_FIRE_A, OBJ_FIRE_B}, {OBJ_TOXIC_A, OBJ_TOXIC_B}, {OBJ_SEED, OBJ_SEED}};
static const unsigned int pick_obj[3] = {OBJ_CANDY, OBJ_POTION, OBJ_CAT};

static void draw(void) {
    unsigned char i, k, f = frame, r = frame;
    unsigned int o;
    obj_begin();
    /* bullets first (if a row is full, they still show), then the enemies,
     * the spells, the rest; each list from another place each frame, so
     * what a full row leaves out flickers rather than vanishes */
    for (k = 0; k < NB; k++) {
        i = (unsigned char)((k + r) % NB);
        if (b_on[i]) obj_add_part(PX(b_x[i]), PX(b_y[i]), bullet_obj[b_kind[i]][(f >> 2) & 1], 5, 6);
    }
    for (k = 0; k < NE; k++) {
        i = (unsigned char)((k + r) % NE);
        if (!e_type[i] || (e_flash[i] & 1)) continue;
        switch (e_type[i]) {
        case E_BAT: o = bat_obj[((f + i * 3) >> 2) & 3]; break;
        case E_GHOST:
            if ((e_t[i] & 64) && (e_t[i] & 32) && (f & 1)) continue;   /* faded */
            o = (f & 16) ? OBJ_GHOST_A : OBJ_GHOST_B;
            break;
        case E_PUMP: o = (f & 8) ? OBJ_PUMP_A : OBJ_PUMP_B; break;
        default: o = (f & 4) ? OBJ_SKULL_A : OBJ_SKULL_B; break;
        }
        obj_add(PX(e_x[i]), PX(e_y[i]), o);
    }
    for (k = 0; k < NS; k++) {
        i = (unsigned char)((k + r) % NS);
        if (s_on[i]) obj_add_part(PX(s_x[i]), PX(s_y[i]), (f & 2) ? OBJ_SHOT_A : OBJ_SHOT_B, 4, 7);
    }
    for (i = 0; i < NX; i++)
        if (x_on[i]) obj_add(x_x[i], x_y[i], (x_kind[i] ? fburst_obj : burst_obj)[x_f[i] >> 2]);
    for (i = 0; i < NP; i++)
        if (p_on[i] && (p_x[i] > 0 || (f & 2))) obj_add(p_x[i], p_y[i], pick_obj[p_kind[i]]);
    obj_end();

    /* the witch: sprites 0-1; blinking while she cannot be hurt */
    if (dead_t || ((invuln & 4) && invuln) || !lives) {
        spr_hide(0);
        spr_hide(1);
    } else {
        wanim = (f >> 3) & 1;
        spr_set(0, wx, wy, wanim ? IMG_WITCH_B0 : IMG_WITCH_A0, 0);
        spr_set(1, wx + 16, wy, wanim ? IMG_WITCH_B1 : IMG_WITCH_A1, 0);
    }
    /* the boss: sprites 2-4 */
    if (boss_on && !boss_hide) {
        signed int sx = PX(bx) + (boss_flash ? ((f & 2) ? 2 : -2) : 0);
        unsigned char fr = boss_kind ? (boss_t & 32) != 0 : (boss_t & 16) != 0;
        unsigned int img = boss_kind == 2 ? (fr ? IMG_COUNT_10 : IMG_COUNT_00)
                         : boss_kind ? (fr ? IMG_LORD_10 : IMG_LORD_00) : (fr ? IMG_KING_10 : IMG_KING_00);
        for (i = 0; i < 3; i++) spr_set(2 + i, sx + i * 16, PX(by), img + i * 6, 1);
    } else
        for (i = 2; i < 5; i++) spr_hide(i);
}

/* ---------------------------------------------------------------- a frame */

static void message(const char *a, const char *b);

static void frame_start(void) {
    video_wait();
    video_scroll();
    video_palette_cycle();
    draw();
    hud(0);
    sfx_update();
    frame++;
    if (chain_t && --chain_t == 0) {
        chain = 0;
        mult = 1;
    }
    if (chain_msg && !(boss_on && boss_t < 60)) {
        if (chain_msg == 50) {
            char t[12] = "CHAIN X ";
            t[7] = (char)('0' + mult);
            message(t, 0);
        }
        if (--chain_msg == 0) message(0, 0);
    }
    /* lightning now and then: a flash, then thunder */
    if (flash_t) {
        if (--flash_t == 0) video_flash(0);
        else if (flash_t == 5 || flash_t == 2) video_flash(1);
        else video_flash(0);
    }
}

static void lightning(void) {
    if (lightning_t) lightning_t--;
    else {
        lightning_t = 900 + (rnd() << 2);
        flash_t = 6;
        sfx_thunder();
    }
}

static void clear_all(void) {
    unsigned char i;
    for (i = 0; i < NE; i++) e_type[i] = 0;
    for (i = 0; i < NS; i++) s_on[i] = 0;
    for (i = 0; i < NB; i++) b_on[i] = 0;
    for (i = 0; i < NP; i++) p_on[i] = 0;
    for (i = 0; i < NX; i++) x_on[i] = 0;
    boss_on = 0;
    boss_max = 0;
}

static unsigned char nothing_left(void) {
    unsigned char i;
    for (i = 0; i < NE; i++)
        if (e_type[i]) return 0;
    return 1;
}

/* wait n frames, the world going on (the witch can fly) */
static void wait_frames(unsigned int n) {
    while (n-- && !quit) {
        frame_start();
        witch();
        movers();
    }
}

static void message(const char *a, const char *b) {
    video_blank(3);
    video_blank(5);
    if (a) video_center(3, a);
    if (b) video_center(5, b);
}

static void pause(void) {
    message("PAUSED", "P TO FLY ON");
    tune_volume(4);
    while (ctl_read() & C_PAUSE) video_wait();
    while (!(ctl_read() & C_PAUSE)) video_wait();
    while (ctl_read() & C_PAUSE) video_wait();
    tune_volume(15);
    message(0, 0);
}

static void night_intro(void) {
    char t[12] = "NIGHT  ";
    t[6] = (char)('0' + night % 10);
    if (night >= 10) {
        t[5] = (char)('0' + night / 10);
    }
    bombs = 3;
    hud(1);
    tune_tempo(0x100);
    PLAY(MIDNIGHT);
    message(t, "THE CLOCK STRIKES TWELVE");
    sfx_cackle();
    flash_t = 6;
    wait_frames(120);
    while (tune_playing() && !quit) wait_frames(1);   /* the chimes and the bell, to the end */
    message(0, 0);
    PLAY(STAGE);
    prog = 0;
#ifdef BOSS_TEST
    prog = NIGHT_FRAMES - 10;      /* (tests: straight to the boss) */
    night = BOSS_TEST;
#endif
    wave_t = 40;
    wave_n = 0;
    lightning_t = 600;
}

static void night_clear(void) {
    unsigned int bonus = 2000 + (unsigned int)lives * 500 + (unsigned int)bombs * 1000;
    unsigned int k;
    spr_hide(2); spr_hide(3); spr_hide(4);
    PLAY(CLEAR);
    message("THE NIGHT IS YOURS!", "BONUS");
    video_num(22, 5, bonus, 5);
    for (k = 0; k < 200 && !quit; k++) {
        frame_start();
        witch();
        movers();
        if (k == 60) {
            add_score(bonus);
            sfx_pickup();
        }
    }
    message(0, 0);
    night++;
    if (night > night_reached) night_reached = night;
}

void game_play(unsigned char is_demo) {
    unsigned char c;
    demo = is_demo;
    demo_t = 0;
    seed ^= frames;
    score = 0;
    night = 1;
    night_reached = 1;
    lives = 3;
    power = 1;
    next_life = 25000;
    quit = 0;
    chain = chain_t = chain_msg = 0;
    mult = 1;
    wx = 20;
    wy = 90;
    invuln = 60;
    dead_t = 0;
    clear_all();
    video_picture(SCR_GAME);
    video_bands_draw();
    bands_on = 1;
    scroll_speed = 0x100;
    video_fade(16);
    night_intro();
    while (!quit) {
        frame_start();
        if (!demo && (ctl_read() & C_PAUSE)) pause();
        witch();
        if (quit) break;
        if (!boss_on) {
            prog++;
            /* the Mountain King gets faster and faster */
            tune_tempo((unsigned int)(0x100 + (prog >> 4) + (night - 1) * 16));
            scroll_speed = 0x100 + (prog >> 5);
            if (prog < NIGHT_FRAMES) {
                if (--wave_t == 0) {
                    wave();
                    c = (unsigned char)(150 - (prog >> 5) - night * 6);
                    wave_t = c < 60 ? 60 : c;
                }
            } else if (nothing_left()) {
                boss_start();
                scroll_speed = 0x80;
            }
            lightning();
        }
        enemies();
        boss();
        movers();
        if (boss_on == 0 && boss_dying == 0 && boss_max && prog >= NIGHT_FRAMES) {
            boss_max = 0;
            scroll_speed = 0x100;
            night_clear();
            if (!quit) night_intro();
        }
        if (!lives && !dead_t) break;
#ifndef DEMO
        if (demo && ++demo_t > 2400) quit = 1;     /* the attract mode: 48 seconds */
#endif
    }
    clear_all();
    spr_hide(0); spr_hide(1); spr_hide(2); spr_hide(3); spr_hide(4);
    if (!lives && !demo) {
        unsigned int k;
        tune_tempo(0x100);
        PLAY(GAMEOVER);
        message("GAME OVER", "REST IN PEACE");
        bands_on = 0;
        for (k = 0; k < 400; k++) {
            frame_start();
            if (k > 100 && (ctl_read() & C_FIRE)) break;
        }
        bands_on = 1;
    }
    tune_stop();
}

signed char sine8(unsigned char a) {
    return sine[a >> 2];
}
