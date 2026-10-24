/* Son Son II sound for the Genesis: native rebuild of the PC Engine driver
 * (banks 0x0A/0x0B) rendered on YM2612 FM + SN76489 noise.  docs/ss2/SOUND.md.
 *
 * Caller contract
 *  - Son Son II owns the YM2612/PSG/Z80 while its mode runs.  Call
 *    snd_shutdown() (SonSon's driver) BEFORE ss2_sound_init(), and
 *    ss2_sound_shutdown() BEFORE handing the chips back (snd_init()).
 *  - ss2_sound_init() loads SGDK's NULL Z80 driver and holds the Z80 bus (the
 *    same model as src/sound.c); YM writes happen only inside
 *    ss2_sound_tick()/init/shutdown, each bracketed by Z80_getAndRequestBus.
 *    No Z80 RAM, no DMA source and no cartridge bank are used.
 *  - ss2_sound_tick() once per displayed frame from the main loop, at the
 *    point where the PCE IRQ handler ran the driver (bank 0 $E16D): skip it on
 *    frames where the source skipped it ($6C|$6D|$C7 nonzero).  On PAL it
 *    advances an extra sequencer frame every 5th call so tempo stays at the
 *    source's 60 Hz rate.  It is not interrupt-safe against
 *    ss2_sound_request(); call both from the same context.
 */
#ifndef SS2_SOUND_H
#define SS2_SOUND_H

#include <genesis.h>

void ss2_sound_init(void);
void ss2_sound_request(u8 id);      /* same ids as the PCE ring ($2285): 0..$2A, $F0-$F2 */
void ss2_sound_tick(void);          /* once per frame */
void ss2_sound_shutdown(void);
void ss2_sound_set_hold(u8 v);      /* mirror of RAM $6B (pause: music muted + frozen) */

/* Request ids used by the game (bank 0/2/3/4/9 call sites, see SOUND.md). */
#define SS2_SND_STOP_ALL    0xF0
#define SS2_SND_STOP_SFX    0xF1
#define SS2_SND_STOP_MUSIC  0xF2
#define SS2_SND_MUS_STAGE(n) ((u8)(3 + (n)))   /* bank 0/4: LDA #3, ADC $69 */
#define SS2_SND_SFX_HURT    0x15
#define SS2_SND_SFX_JUMP    0x16
#define SS2_SND_SFX_STAFF   0x19
#define SS2_SND_SFX_PAUSE   0x1A

#endif
