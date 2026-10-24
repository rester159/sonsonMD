/* ss2_ram.h -- SON SON II game state, laid out as the HuCard's work RAM.
 *
 * The native logic keeps the source's 8 KiB work-RAM layout (logical
 * $2000-$3FFF) as one byte image so multi-byte carry chains, shared arrays
 * and out-of-range table reads behave exactly as on the PC Engine, and so
 * every routine can be validated byte-for-byte against the research trace
 * (tools/ss2/hosttest). The code operating on it is native C; nothing here
 * executes or interprets HuC6280 instructions.
 *
 * Names follow demonstrated use; '?' marks fields whose meaning is not yet
 * established.
 */
#ifndef SS2_RAM_H
#define SS2_RAM_H

#include <genesis.h>

extern u8 ss2_ram[0x2000];

#define R(a)        (ss2_ram[(u16)((a) - 0x2000) & 0x1FFF])
#define ZP(a)       R(0x2000 + (a))

/* ---- zero page ---- */
#define Z_FRAME     ZP(0x12)    /* frame counter                          */
#define Z_PAD       ZP(0x16)    /* held: L D R U Run Sel II I (bit7..0)   */
#define Z_PRESS     ZP(0x20)    /* newly pressed, same layout             */
#define Z_CAMX_HI   ZP(0x45)    /* camera x: $45 (3 bits) : $46 : $47 frac */
#define Z_CAMX_LO   ZP(0x46)
#define Z_CAMX_FR   ZP(0x47)
#define Z_CAMY_HI   ZP(0x48)
#define Z_CAMY_LO   ZP(0x49)
#define Z_CAMY_FR   ZP(0x4A)
#define Z_MT_LO     ZP(0x5D)    /* metatile table base (bank 6/7 $C000+)  */
#define Z_MT_HI     ZP(0x5E)
#define Z_CAMLOCK   ZP(0x5F)    /* camera locked: player moves on screen  */
#define Z_VLOCK     ZP(0x60)    /* vertical camera hold (airborne)        */
#define Z_HIT_VINE  ZP(0x61)    /* probe met attribute 2 (climbable)      */
#define Z_HIT_3     ZP(0x62)    /* probe met attribute 3                  */
#define Z_HIT_HURT  ZP(0x63)    /* probe met attribute 4 (hazard)         */
#define Z_HSCROLL   ZP(0x64)    /* horizontal follow with 0x40..0xA0 zone */
#define Z_TIME_M    ZP(0x66)    /* play clock, BCD                        */
#define Z_TIME_S    ZP(0x67)
#define Z_TIME_F    ZP(0x68)
#define Z_STAGE     ZP(0x69)
#define Z_STAFFPAL  ZP(0x72)    /* staff segment attribute selector       */
#define Z_INLOCK1   ZP(0x7F)    /* ? input lock                           */
#define Z_DEMOFX    ZP(0x86)    /* ? set by bank 3 $8302                  */
#define Z_INLOCK2   ZP(0x95)    /* ? input lock                           */

/* ---- player block $2296-$22EB ---- */
#define P_MODE      R(0x2297)   /* $80 normal, $40 dying, $20 hurt        */
#define P_XH        R(0x2298)   /* screen x : hi, lo, frac                */
#define P_XL        R(0x2299)
#define P_XF        R(0x229A)
#define P_YH        R(0x229B)
#define P_YL        R(0x229C)
#define P_YF        R(0x229D)
#define P_ATTRH     R(0x229E)   /* SATB attr hi: bit3 = x flip            */
#define P_ATTRL     R(0x229F)   /* SATB attr lo: palette                  */
#define P_FRAMEH    R(0x22A0)   /* pattern pointer in bank $10            */
#define P_FRAMEL    R(0x22A1)
#define P_VXH       R(0x22A2)   /* velocities, signed 8.8                 */
#define P_VXL       R(0x22A3)
#define P_VYH       R(0x22A4)
#define P_VYL       R(0x22A5)
#define P_PROBEX    R(0x22A6)   /* last collision probe offset            */
#define P_PROBEY    R(0x22A7)
#define P_JUMPKIND  R(0x22A8)   /* index into the $DAF7 jump velocities   */
#define P_JUMPREQ   R(0x22A9)
#define P_JUMPING   R(0x22AA)
#define P_ATTACKREQ R(0x22AB)
#define P_JUMPHIGH  R(0x22AC)   /* rose 0x30 px: steering enabled         */
#define P_ATTACKING R(0x22AD)
#define P_ONVINE    R(0x22AE)
#define P_CLIMBSTEP R(0x22AF)
#define P_SETTLE    R(0x22B0)   /* landed: re-centre camera on y = $A0    */
#define P_STANDING  R(0x22B1)
#define P_ANIMSET   R(0x22B2)   /* even index into $DB23                  */
#define P_ANIMTICK  R(0x22B3)
#define P_STAFFMAX  R(0x22B5)
#define P_ATKDIR    R(0x22B6)
#define P_JUMPDIR   R(0x22B7)
#define P_LIFE      R(0x22B8)
#define P_MAGIC     R(0x22BB)
#define P_MAGICSEL  R(0x22C0)
#define P_FLAGSC1   R(0x22C1)
#define P_CLOUDREQ  R(0x22C2)
#define P_ONCLOUD   R(0x22C3)
#define P_C4        R(0x22C4)   /* ?                                      */
#define P_PUSHOBJ   R(0x22C5)   /* 1-based object pushing the player      */
#define P_C6        R(0x22C6)
#define P_HITOBJ    R(0x22C7)   /* 1-based object that hit the player     */
#define P_HURT      R(0x22C8)   /* hurt/knockback timer                   */
#define P_DEATH     R(0x22D4)
#define P_DEATHPTRL R(0x22D5)
#define P_DEATHPTRH R(0x22D6)
#define P_NOPAD     R(0x22D7)   /* $88 while no direction is held         */
#define P_DIR       R(0x22D8)   /* 0 R, 1 DR, 2 D, 3 DL, 4 L, 5 U         */
#define P_LASTDIR   R(0x22D9)
#define P_JUMPY     R(0x22DA)   /* screen y at take-off                   */

/* sound-effect request ring ($BBF5) */
#define SFX_RING(i) R(0x2285 + (i))
#define SFX_HEAD    R(0x2296)

static inline s16 ss2_vx(void) { return (s16)((P_VXH << 8) | P_VXL); }
static inline s16 ss2_vy(void) { return (s16)((P_VYH << 8) | P_VYL); }
static inline void ss2_set_vx(u16 v) { P_VXH = (u8)(v >> 8); P_VXL = (u8)v; }
static inline void ss2_set_vy(u16 v) { P_VYH = (u8)(v >> 8); P_VYL = (u8)v; }

#endif
