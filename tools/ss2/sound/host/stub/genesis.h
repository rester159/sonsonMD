/* Host stub of the few SGDK calls used by src/ss2_sound.c (render test). */
#ifndef SS2_HOST_GENESIS_STUB_H
#define SS2_HOST_GENESIS_STUB_H
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int16_t s16;
typedef int bool;
#define TRUE 1
#define FALSE 0
#define Z80_DRIVER_NULL 0
#define PSG_ENVELOPE_MIN 15
#define PSG_NOISE_TYPE_WHITE 1
#define PSG_NOISE_FREQ_TONE3 3
void YM2612_writeReg(const u16 part, const u8 reg, const u8 data);
void PSG_setEnvelope(u8 channel, u8 value);
void PSG_setTone(u8 channel, u16 value);
void PSG_setNoise(u8 type, u8 frequency);
void Z80_loadDriver(const u16 driver, const bool waitReady);
void Z80_requestBus(bool wait);
bool Z80_getAndRequestBus(bool wait);
void Z80_releaseBus(void);
u16 SYS_isPAL(void);
#endif
