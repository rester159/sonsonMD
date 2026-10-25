/* Host shim: just the SGDK scalar types the SS2 logic uses. */
#ifndef HOST_GENESIS_SHIM_H
#define HOST_GENESIS_SHIM_H
#include <stdint.h>
#include <stddef.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;
typedef int8_t s8; typedef int16_t s16; typedef int32_t s32;
typedef volatile uint8_t vu8; typedef volatile uint16_t vu16; typedef volatile uint32_t vu32;
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
typedef u8 bool_;
/* cartridge SRAM: blank (as a fresh cartridge) in host runs */
static u8 host_sram[0x8000];
static inline void SRAM_enable(void) {}
static inline void SRAM_enableRO(void) {}
static inline void SRAM_disable(void) {}
static inline u8 SRAM_readByte(u32 a) { return host_sram[a & 0x7FFF]; }
static inline void SRAM_writeByte(u32 a, u8 v) { host_sram[a & 0x7FFF] = v; }
#endif
