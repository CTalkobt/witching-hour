/* main.c — Witching Hour: the title, the hall of fame, and the game
 *
 * The title plays Gounod's Funeral March of a Marionette while the witch
 * crosses the moon and bats flit about; its pages take turns (how to play,
 * who is who, the hall of fame). Left alone it flies a demo. A score good
 * enough gets three initials in the hall of fame, which is saved on the
 * disk (WITCHHOF) and loaded again at the start.
 */
#include <cbm.h>
#include <input.h>
#include <audio_tune.h>
#include "hw.h"
#include "video.h"
#include "music.h"
#include "sfx.h"
#include "game.h"

#define NHS 5
#define HOF_MAGIC 0x57               /* "W": the file is ours */
static struct {
    unsigned char magic;
    unsigned long score[NHS];
    char name[NHS][4];
    unsigned char night[NHS];
    unsigned char sum;
} hof = {HOF_MAGIC, {30000, 20000, 15000, 10000, 5000}, {"GRM", "JSB", "FCH", "CHG", "EAP"},
         {3, 2, 2, 1, 1}, 0};
#define hs_score hof.score
#define hs_name hof.name
#define hs_night hof.night
unsigned long hiscore = 30000;

/* the hall of fame on the disk the game came from */
static unsigned char hof_sum(const unsigned char *p) {
    unsigned char i, s = 0x5A;
    for (i = 0; i < sizeof hof - 1; i++) s = (unsigned char)((s << 1 | s >> 7) + p[i]);
    return s;
}

static unsigned char drive(void) {
    unsigned char d = *(volatile unsigned char *)0xBA;
    return d < 8 ? 8 : d;
}

/* the file's name, upper case on the disk (cc45's strings are PETSCII's
 * shifted letters): "WITCHHOF", after "S0:", the DOS command that scratches
 * it (a save scratches the old file first rather than trusting the DOS's
 * save-with-replace, "@0:") */
static const char hof_file[] = {0x53, 0x30, 0x3A, 0x57, 0x49, 0x54, 0x43, 0x48, 0x48, 0x4F, 0x46, 0};

static void hof_load(void) {
    static unsigned char buf[sizeof hof];
    unsigned char i;
    if (cbm_load(hof_file + 3, drive(), buf) != sizeof hof) return;
    if (buf[0] != HOF_MAGIC || buf[sizeof hof - 1] != hof_sum(buf)) return;
    for (i = 0; i < sizeof hof; i++) ((unsigned char *)&hof)[i] = buf[i];
    hiscore = hs_score[0];
}

static void hof_save(void) {
    hof.sum = hof_sum((const unsigned char *)&hof);
    tune_stop();
    video_kernal(1);
    cbm_k_setlfs(15, drive(), 15);          /* the command channel: scratch */
    cbm_k_setnam(hof_file);
    cbm_k_open();                           /* (no file yet: an error, ignored) */
    cbm_k_close(15);
    cbm_save(hof_file + 3, drive(), &hof, sizeof hof);
    video_kernal(0);
}

#define TROW 16                      /* the title's text rows: 16-22 */

static void clear_text(void) {
    unsigned char r;
    for (r = TROW; r < TROW + 7; r++) video_clear_row(r);
}

static void page_controls(void) {
    clear_text();
    video_center(TROW, "PRESS FIRE TO FLY");
    video_center(TROW + 2, "JOYSTICK, CURSORS OR WASD");
    video_center(TROW + 3, "FIRE OR SPACE: CAST A SPELL");
    video_center(TROW + 4, "B OR FIRE 2: THE MIDNIGHT TOLL");
    video_center(TROW + 6, "P PAUSE   RUN/STOP QUIT");
}

static void page_cast(void) {
    clear_text();
    video_center(TROW, "THE NIGHT'S GUESTS");
    video_text(6, TROW + 2, "BAT    100");
    video_text(6, TROW + 4, "GHOST  200");
    video_text(6, TROW + 6, "SKULL  250");
    video_text(24, TROW + 2, "LANTERN 300");
    video_text(24, TROW + 4, "CANDY   500");
    video_text(24, TROW + 6, "POTION POWER");
}

static void page_chain(void) {
    clear_text();
    video_center(TROW, "CHAINS AND MASTERS");
    video_center(TROW + 2, "KILL QUICKLY: A CHAIN OF KILLS");
    video_center(TROW + 3, "MULTIPLIES THEIR POINTS, UP TO X5");
    video_center(TROW + 5, "THE PUMPKIN KING, THE BONE LORD");
    video_center(TROW + 6, "AND THE VAMPIRE COUNT AWAIT");
}

static void page_scores(void) {
    unsigned char i;
    clear_text();
    video_center(TROW, "HALL OF FAME");
    for (i = 0; i < NHS; i++) {
        video_num(9, TROW + 2 + i, i + 1, 1);
        video_text(11, TROW + 2 + i, hs_name[i]);
        video_num(16, TROW + 2 + i, hs_score[i], 7);
        video_text(25, TROW + 2 + i, "NIGHT");
        video_num(31, TROW + 2 + i, hs_night[i], 1);
    }
}

static void page_credits(void) {
    clear_text();
    video_center(TROW, "MUSIC FROM THE MASTERS");
    video_center(TROW + 1, "GOUNOD  FUNERAL MARCH OF A MARIONETTE");
    video_center(TROW + 2, "GRIEG  HALL OF THE MOUNTAIN KING");
    video_center(TROW + 3, "BACH  TOCCATA IN D MINOR");
    video_center(TROW + 4, "BERLIOZ  DIES IRAE, WITCHES' SABBATH");
    video_center(TROW + 5, "CHOPIN  FUNERAL MARCH");
    video_center(TROW + 6, "WESTMINSTER CHIMES AT MIDNIGHT");
}

/* the title: until FIRE (1) or time for the demo (0) */
static unsigned char title(void) {
    unsigned int t;
    unsigned char page = 0, k, f, i;
    signed int witch_x = -40, bat_x[4], bat_y[4];
    for (k = 0; k < 4; k++) {
        bat_x[k] = (signed int)(rnd() % 300);
        bat_y[k] = 70 + (rnd() & 31);
    }
    bands_on = 0;
    video_picture(SCR_TITLE);
    video_fade(16);
    tune_tempo(0x100);
    PLAY(TITLE);
    page_controls();
    input_flush();
    while (ctl_read() & C_FIRE) video_wait();
    for (t = 0;; t++) {
        video_wait();
        video_title_scroll();
        video_palette_cycle();
        sfx_update();
        f = (unsigned char)t;
        /* the witch across the moon, now and then; bats about */
        if (witch_x < 400) witch_x++;
        if ((t & 1023) == 0) witch_x = -40;
        spr_set(0, witch_x, 34 + (sine8(f) >> 2), (f & 8) ? IMG_WITCH_A0 : IMG_WITCH_B0, 0);
        spr_set(1, witch_x + 16, 34 + (sine8(f) >> 2), (f & 8) ? IMG_WITCH_A1 : IMG_WITCH_B1, 0);
        obj_begin();
        for (k = 0; k < 4; k++) {
            bat_x[k] -= 1 + (k & 1);
            if (bat_x[k] < -20) {
                bat_x[k] = 320;
                bat_y[k] = 60 + (rnd() & 63);
            }
            obj_add(bat_x[k], bat_y[k] + (sine8((unsigned char)(f * 3 + k * 50)) >> 3),
                    ((f + k * 5) & 8) ? OBJ_BAT_0 : OBJ_BAT_2);
        }
        if (page == 1) {
            /* the guests beside their names */
            obj_add(28, (TROW + 2) * 8 - 4, (f & 8) ? OBJ_BAT_0 : OBJ_BAT_2);
            obj_add(28, (TROW + 4) * 8 - 4, (f & 16) ? OBJ_GHOST_A : OBJ_GHOST_B);
            obj_add(28, (TROW + 6) * 8 - 4, (f & 4) ? OBJ_SKULL_A : OBJ_SKULL_B);
            obj_add(172, (TROW + 2) * 8 - 4, (f & 8) ? OBJ_PUMP_A : OBJ_PUMP_B);
            obj_add(172, (TROW + 4) * 8 - 4, OBJ_CANDY);
            obj_add(172, (TROW + 6) * 8 - 4, OBJ_POTION);
        }
        obj_end();
        if (page == 0) {
            if ((t & 31) == 0) video_center(TROW, "PRESS FIRE TO FLY");
            if ((t & 31) == 20) video_clear_row(TROW);
        }
        if (t % 300 == 299) {
            page = (unsigned char)((page + 1) % 5);
            switch (page) {
            case 0: page_controls(); break;
            case 1: page_cast(); break;
            case 2: page_chain(); break;
            case 3: page_scores(); break;
            case 4: page_credits(); break;
            }
            if (t > 2900) break;
        }
        i = ctl_read();
        if (i & C_FIRE) {
            spr_hide(0);
            spr_hide(1);
            sfx_cackle();
            return 1;
        }
    }
    spr_hide(0);
    spr_hide(1);
    return 0;
}

/* three initials: up and down choose a letter, FIRE takes it */
static void enter_name(unsigned char slot) {
    unsigned char n = 0, c = 'A', d, held = 0, wait = 0;
    char cur[2] = "A";
    clear_text();
    video_center(TROW, "A NEW NAME FOR THE HALL OF FAME");
    video_center(TROW + 2, "UP, DOWN AND FIRE");
    hs_name[slot][0] = hs_name[slot][1] = hs_name[slot][2] = '.';
    hs_name[slot][3] = 0;
    while (ctl_read() & C_FIRE) video_wait();
    while (n < 3) {
        video_wait();
        video_title_scroll();
        video_palette_cycle();
        sfx_update();
        hs_name[slot][n] = (char)c;
        video_text(17, TROW + 4, hs_name[slot]);
        cur[0] = (frames & 16) ? '-' : ' ';
        video_text((unsigned char)(17 + n), TROW + 5, cur);
        d = ctl_read();
        if (wait) wait--;
        if (!wait && (d & C_UP)) { c = c == 'Z' ? 'A' : c + 1; wait = 8; sfx_tick(); }
        if (!wait && (d & C_DOWN)) { c = c == 'A' ? 'Z' : c - 1; wait = 8; sfx_tick(); }
        if ((d & C_FIRE) && !held) {
            video_text((unsigned char)(17 + n), TROW + 5, " ");
            n++;
            c = 'A';
            sfx_pickup();
        }
        held = d & C_FIRE;
    }
}

static void high_score(void) {
    unsigned char i, j;
    if (score <= hs_score[NHS - 1]) return;
    for (i = 0; i < NHS && hs_score[i] >= score; i++) {}
    for (j = NHS - 1; j > i; j--) {
        hs_score[j] = hs_score[j - 1];
        hs_night[j] = hs_night[j - 1];
        hs_name[j][0] = hs_name[j - 1][0];
        hs_name[j][1] = hs_name[j - 1][1];
        hs_name[j][2] = hs_name[j - 1][2];
    }
    hs_score[i] = score;
    hs_night[i] = night_reached;
    if (score > hiscore) hiscore = score;
    bands_on = 0;
    video_picture(SCR_TITLE);
    PLAY(TITLE);
    enter_name(i);
    clear_text();
    video_center(TROW + 2, "SAVING");
    video_wait();
    hof_save();
}

int main(void) {
    if (!gfx_load()) return 1;
    hof_load();
    sfx_on();
    tune_voices(0);
    video_on();
#ifdef HOF_TEST
    hs_score[0] += 1111;
    hof_save();
#endif
#ifdef DEMO
    for (;;) game_play(1);
#endif
    for (;;) {
        if (title()) {
            game_play(0);
            high_score();
            if (ctl_read() & C_QUIT) break;
        } else
            game_play(1);
    }
    tune_stop();
    video_off();
    sfx_off();
    return 0;
}
