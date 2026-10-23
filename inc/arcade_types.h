#ifndef ARCADE_TYPES_H
#define ARCADE_TYPES_H
#ifdef SGDK_GCC
#include <genesis.h>
#else
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int16_t s16;
typedef int32_t s32;
#endif
#endif
