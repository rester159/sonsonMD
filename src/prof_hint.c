/* Diagnostic-only statistical profiler (build with -DPROF_HINT; never a
 * release ROM). HInt fires every 8th active line and records the interrupted
 * PC into a ring the headless harness drains once per refresh. */
#ifdef PROF_HINT
#include <genesis.h>
#define PROF_RING 256
__attribute__((used,externally_visible)) volatile u32 prof_ring[PROF_RING];
__attribute__((used,externally_visible)) volatile u32 *prof_ptr = prof_ring;
__attribute__((used,externally_visible)) volatile u16 prof_cnt[8];
extern void prof_hint(void);
__asm__(
"    .globl prof_hint\n"
"prof_hint:\n"
"    move.l %a0,-(%sp)\n"
"    movea.l prof_ptr,%a0\n"
"    move.l 6(%sp),(%a0)+\n"
"    cmpa.l #prof_ring+1024,%a0\n"
"    bcs.s 1f\n"
"    lea prof_ring,%a0\n"
"1:  move.l %a0,prof_ptr\n"
"    move.l (%sp)+,%a0\n"
"    rte\n");
void prof_start(void)
{
    SYS_setHIntCallback(prof_hint);
    VDP_setHIntCounter(7);
    VDP_setHInterrupt(TRUE);
}
#else
void prof_start(void) {}
#endif
