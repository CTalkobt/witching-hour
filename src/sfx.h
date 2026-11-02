/* sfx.h — sound effects: SID 2 (voices 3-5, mirrored on SID 4) and samples
 * on the audio DMA (the cackle, thunder, a boom). The music has SID 1. */
#pragma once

void sfx_on(void);
void sfx_off(void);
void sfx_update(void);               /* once a frame */
void sfx_shot(void);                 /* the witch's spell */
void sfx_hit(void);                  /* an enemy hit, not dead */
void sfx_pop(void);                  /* an enemy gone (a bat, a ghost) */
void sfx_crunch(void);               /* an enemy gone (a pumpkin, a skull) */
void sfx_enemy_shot(void);
void sfx_pickup(void);               /* candy */
void sfx_power(void);                /* a potion */
void sfx_life(void);                 /* the cat: a life */
void sfx_screech(void);              /* bats coming */
void sfx_charge(void);               /* a skull charges */
void sfx_boss_hit(void);
void sfx_dead(void);                 /* the witch hit */
void sfx_bomb(void);                 /* the midnight toll */
void sfx_cackle(void);
void sfx_thunder(void);
void sfx_boom(void);
void sfx_tick(void);                 /* a menu blip */
