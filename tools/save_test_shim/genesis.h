#ifndef SAVE_TEST_GENESIS_H
#define SAVE_TEST_GENESIS_H
#include <stdint.h>
#include <stdbool.h>
typedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;typedef int8_t s8;
#define TRUE true
#define FALSE false
void SRAM_enable(void);void SRAM_enableRO(void);void SRAM_disable(void);
u8 SRAM_readByte(u32 offset);void SRAM_writeByte(u32 offset,u8 value);
#endif
