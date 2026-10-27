/* SonSon audio: recorded AY register streams, advanced in VBlank and mapped
 * to Genesis PSG (chip B) and YM2612 (chip A and layered cues).
 * This is a synthesis approximation, not an identical AY implementation.
 */
#ifndef SONSON_NATIVE_SOUND_H
#define SONSON_NATIVE_SOUND_H

#include <genesis.h>

/* These are main-CPU event selectors ($6339/$6341), translated by snd_command
 * through the original $63DD table. Replay data uses the resulting AY driver
 * latch bytes. Keep the two namespaces distinct. */
#define SND_CMD_1UP         0x03    /* latch 04: extra-life cue */
#define SND_1UP_ENABLE      1       /* enable the layered extra-life cue */

/* Gameplay event selectors. */
#define SND_CMD_STOP        0x00    /* halt + silence (arcade pairs 00+song)     */
#define SND_CMD_COIN        0x01    /* coin insert jingle                        */
#define SND_CMD_FIRE        0x08    /* player shot SFX  (CONFIRMED)              */
#define SND_CMD_ENEMY       0x0C    /* enemy-defeat SFX (VERIFIED 2026-06-17)    */
#define SND_CMD_ITEM        0x04    /* low-value item/pickup SFX (VERIFIED +10)  */
#define SND_CMD_BONUS       0x09    /* high-value pickup/bonus SFX (VERIFIED +500)*/
#define SND_CMD_DEATH       0x14    /* player-death SFX (VERIFIED 2026-06-17)    */
#define SND_CMD_BGM_GAME    0x18    /* main in-game level BGM (CONFIRMED)       */
#define SND_CMD_ROUND_A     0x16    /* round-intro music part A                  */
#define SND_CMD_ROUND_B     0x0F    /* round-intro music part B (round selector) */

/* Initialise the audio subsystem: park the Z80 (NULL driver, bus held so the
 * 68k YM writes land), set up the YM SFX voice, clear the replay slots, and
 * install the V-Blank callback that advances the replay. Call ONCE when entering
 * the game -- AFTER the CAPCOM splash (which uses + resets the YM directly and
 * returns the Z80 to SGDK's clean halt). Safe to call again on re-entry. */
void snd_init(void);

/* Stop all audio + silence the chips, and (optionally) detach the V-Blank
 * callback. Call when leaving the game back to the front-end so the splashes own
 * the sound hardware again. */
void snd_shutdown(void);

/* Fire a gameplay event selector (the arcade $6347 queue value). Routes music commands
 * to the music slot + SFX to the SFX slot, exactly like the arcade driver; an
 * SFX never disturbs the running music (separate chips / masked mixer). Cheap:
 * just arms the replay stream pointer(s); the V-Blank tick does the work. */
void snd_command(u8 cmd);
/* 6400/6339: caller must be outside live gameplay; ignores demo-sound DIP. */
void snd_play_coin(void);

/* Play the extra-life / 1-up jingle (cmd 0x04) LAYERED over the BGM, on the
 * YM2612's second FM bank -- the music + SFX are untouched. One-shot (~3.2 s).
 * snd_command(SND_CMD_1UP) reaches the same layered cue through translation. */
void snd_play_1up(void);

/* Convenience wrappers for the wired game events (clearer call sites). */
static inline void snd_play_bgm(void)  { snd_command(SND_CMD_BGM_GAME); }
static inline void snd_sfx_fire(void)  { snd_command(SND_CMD_FIRE); }
static inline void snd_sfx_enemy(void) { snd_command(SND_CMD_ENEMY); }  /* enemy defeat   */
static inline void snd_sfx_item(void)  { snd_command(SND_CMD_ITEM); }   /* low-value pickup  */
static inline void snd_sfx_bonus(void) { snd_command(SND_CMD_BONUS); }  /* high-value pickup */
static inline void snd_sfx_death(void) { snd_command(SND_CMD_DEATH); }  /* player death   */
static inline void snd_stop(void)      { snd_command(SND_CMD_STOP); }

/* Round-intro music (arcade plays 0x16 then 0x10 at a round/level start). */
static inline void snd_play_round(void) { snd_command(SND_CMD_ROUND_A); snd_command(SND_CMD_ROUND_B); }

#endif /* SONSON_NATIVE_SOUND_H */
