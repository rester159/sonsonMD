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
#endif
