/* sfx.c — sound effects
 *
 * SID 2's three voices (3-5 of the twelve; mirrored on SID 4, so both
 * sides), each a sweep: a start frequency changed every frame, with a
 * wobble. Voice 3: noise (hits, crunches); voice 4: blips (the spell,
 * pickups); voice 5: tones (shots, screeches). Samples (WITCHGFX3, bank 1)
 * play on audio DMA channels 0-2.
 */
#include <audio_sid.h>
#include "hw.h"
#include "gfx.h"
#include "sfx.h"

#define V_NOISE 3
#define V_BLIP  4
#define V_TONE  5

static const sid_instrument_t i_noise = { SID_NOISE, 0x08, 0x00, 0 };
static const sid_instrument_t i_noise2 = { SID_NOISE, 0x0A, 0x00, 0 };
static const sid_instrument_t i_blip  = { SID_PULSE, 0x04, 0x00, 0x0400 };
static const sid_instrument_t i_tri   = { SID_TRIANGLE, 0x06, 0x00, 0 };
static const sid_instrument_t i_saw   = { SID_SAWTOOTH, 0x07, 0x00, 0 };

static unsigned char t[6], wob[6];
static unsigned int f[6];
static signed int df[6];

static void start(unsigned char v, const sid_instrument_t *inst, unsigned int freq,
                  signed int step, unsigned char frames, unsigned char wobble) {
    sid_gate(v, 0);
    sid_set_instrument(v, inst);
    sid_set_freq_reg(v, freq);
    sid_gate(v, 1);
    f[v] = freq;
    df[v] = step;
    t[v] = frames;
    wob[v] = wobble;
}

extern unsigned char snd_ch, snd_vol, snd_addr[3], snd_rate[3];
extern unsigned int snd_len;
void snd_on(void);
void snd_off(void);
void snd_play(void);

static void sample(unsigned char ch, unsigned long at, unsigned int len, unsigned long rate, unsigned char vol) {
    snd_ch = ch;
    snd_addr[0] = (unsigned char)at;
    snd_addr[1] = (unsigned char)(at >> 8);
    snd_addr[2] = (unsigned char)(at >> 16);
    snd_len = len;
    snd_rate[0] = (unsigned char)rate;
    snd_rate[1] = (unsigned char)(rate >> 8);
    snd_rate[2] = (unsigned char)(rate >> 16);
    snd_vol = vol;
    snd_play();
}

void sfx_on(void) {
    sid_init();
    sid_set_mirror(1);
    sid_set_volume(15);
    snd_on();
}

void sfx_off(void) {
    snd_off();
    sid_init();
}

void sfx_update(void) {
    unsigned char v;
    for (v = V_NOISE; v <= V_TONE; v++)
        if (t[v]) {
            f[v] += (unsigned int)df[v];
            if (wob[v]) sid_set_freq_reg(v, f[v] + ((t[v] & 2) ? wob[v] * 16 : 0));
            else sid_set_freq_reg(v, f[v]);
            if (--t[v] == 0) sid_gate(v, 0);
        }
}

void sfx_shot(void) { start(V_BLIP, &i_blip, 0x3800, -0x280, 7, 0); }
void sfx_hit(void) { start(V_NOISE, &i_noise, 0x6000, -0x400, 4, 0); }
void sfx_pop(void) { start(V_NOISE, &i_noise, 0x2800, -0x180, 12, 0); }
void sfx_crunch(void) { start(V_NOISE, &i_noise2, 0x1400, -0x60, 22, 0); }
void sfx_enemy_shot(void) { start(V_TONE, &i_tri, 0x0E00, -0x60, 10, 0); }
void sfx_pickup(void) { start(V_BLIP, &i_blip, 0x2000, 0x300, 10, 0); }
void sfx_power(void) { start(V_TONE, &i_blip, 0x0C00, 0x120, 40, 24); }
void sfx_life(void) { start(V_BLIP, &i_tri, 0x1800, 0x180, 30, 40); }
void sfx_screech(void) { start(V_TONE, &i_saw, 0x5000, -0x180, 14, 30); }
void sfx_charge(void) { start(V_TONE, &i_saw, 0x0600, 0x90, 24, 0); }
void sfx_boss_hit(void) { start(V_TONE, &i_blip, 0x0500, -0x10, 4, 0); }
void sfx_dead(void) { start(V_TONE, &i_saw, 0x2400, -0x90, 40, 20); }
void sfx_tick(void) { start(V_BLIP, &i_blip, 0x2800, 0, 3, 0); }

void sfx_cackle(void) { sample(0, SND_CACKLE, SND_CACKLE_LEN, SND_CACKLE_RATE, 230); }
void sfx_thunder(void) { sample(1, SND_THUNDER, SND_THUNDER_LEN, SND_THUNDER_RATE, 255); }
void sfx_boom(void) { sample(2, SND_BOOM, SND_BOOM_LEN, SND_BOOM_RATE, 255); }

void sfx_bomb(void) {
    sfx_thunder();
    start(V_NOISE, &i_noise2, 0x0C00, -0x10, 50, 0);
}
