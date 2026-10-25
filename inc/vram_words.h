/* VBlank list of small VRAM word writes (map cells). Queueing a one- or
 * two-word transfer through the SGDK DMA queue costs ~400 CPU cycles plus
 * per-transfer VBlank setup; these overlays are applied with direct CPU
 * writes right after the frame's DMA queue flush, preserving queue order
 * (streamed columns first, overlays after). Sources must stay valid until
 * the flush, exactly as for queued DMA. */
#ifndef VRAM_WORDS_H
#define VRAM_WORDS_H
/* Include after <genesis.h>. */
/* Queue count words from src to VRAM addr, addr advancing by stride bytes per
 * word. Returns 0 (nothing queued) when the list is full. */
u8 vram_words_queue(const u16 *src,u16 addr,u16 count,u16 stride);
/* Nonzero when n more entries fit (for all-or-nothing publications). */
u8 vram_words_room(u16 n);
/* Apply and clear the list. Call only during VBlank, after DMA_flushQueue. */
void vram_words_flush(void);
#endif
