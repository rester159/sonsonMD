/* Target-only fixture: actual 68000 probe vs C oracle, plus ABI canaries.
 * Build as main.c with SS2_PROBECHECK; not a shipping game entry point. */
#include <genesis.h>
#include "ss2_logic.h"
#include "ss2_ram.h"
volatile u16 probe_done, probe_guard_failures, probe_abi_failures;
volatile u32 motion_failures;
u32 probe_abi_regs[11] __attribute__((used, externally_visible));
static u8 bank6[8192], bank7[8192], expected_ram[8192];
extern void probe_abi(void);
__asm__(".text\n.globl probe_abi\nprobe_abi:\n"
" movem.l %d2-%d7/%a2-%a6,-(%sp)\n"
" move.l #0x12340002,%d2\n move.l #0x12340003,%d3\n"
" move.l #0x12340004,%d4\n move.l #0x12340005,%d5\n"
" move.l #0x12340006,%d6\n move.l #0x12340007,%d7\n"
" move.l #0x12340008,%a2\n move.l #0x12340009,%a3\n"
" move.l #0x1234000a,%a4\n move.l #0x1234000b,%a5\n"
" move.l #0x1234000c,%a6\n"
" move.l #0xffff,-(%sp)\n move.l #0x8000,-(%sp)\n"
" jsr ss2_probe_point_native\n addq.l #8,%sp\n"
" lea ss2_ram,%a0\n move.l %a0,-(%sp)\n"
" jsr ss2_obj_motion_native\n addq.l #4,%sp\n"
" movem.l %d2-%d7/%a2-%a6,probe_abi_regs\n"
" movem.l (%sp)+,%d2-%d7/%a2-%a6\n rts\n");
int main(bool hardReset)
{
    u32 n; u16 i; (void)hardReset;
    for(i=0;i<8192;i++) {
        bank6[i]=(u8)(i^(i>>4)); bank7[i]=(u8)(~i^(i>>5));
        ss2_ram[i]=(u8)(i*13+(i>>4));
    }
    ss2_banks[6]=bank6; ss2_banks[7]=bank7;
    probe_abi();
    for(i=0;i<11;i++) if(probe_abi_regs[i] != 0x12340002UL+i)probe_abi_failures++;
    for(n=0;n<65536;n++) {
        u16 sx=(u16)n, sy=(u16)(n*40503UL);
        ss2_ram[0x45]=(u8)(n>>3); ss2_ram[0x46]=(u8)(n*7);
        ss2_ram[0x48]=(u8)(n>>5); ss2_ram[0x49]=(u8)(n*11);
        ss2_ram[0x5d]=(u8)(n*17); ss2_ram[0x5e]=(u8)(n>>8);
        ss2_ram[0x69]=(u8)n;
        (void)ss2_probe_point(sx,sy);
        if(!(n&255)) {
            /* Repeat the same native probe: only its documented scratch
             * bytes may change, and they already contain this result. */
            for(i=0;i<8192;i++) expected_ram[i]=ss2_ram[i];
            (void)ss2_probe_point_native(sx,sy);
            for(i=0;i<8192;i++) if(expected_ram[i]!=ss2_ram[i])probe_guard_failures++;
        }
        {
            u8 *o=&ss2_ram[n&31];
            u32 px=(n*7919UL)&0xffffff, py=(~(n*40503UL))&0xffffff;
            u16 vx=(u16)n, vy=(u16)~n;
            u32 ex=(px+(s16)vx)&0xffffff, ey=(py+(s16)vy)&0xffffff;
            o[0x47a]=(u8)(px>>16); o[0x49a]=(u8)(px>>8); o[0x4ba]=(u8)px;
            o[0x4da]=(u8)(py>>16); o[0x4fa]=(u8)(py>>8); o[0x51a]=(u8)py;
            o[0x5ba]=(u8)(vx>>8); o[0x5da]=(u8)vx;
            o[0x5fa]=(u8)(vy>>8); o[0x61a]=(u8)vy;
            if(!(n&255)) {
                u8 *e=&expected_ram[n&31];
                for(i=0;i<8192;i++)expected_ram[i]=ss2_ram[i];
                e[0x47a]=(u8)(ex>>16);e[0x49a]=(u8)(ex>>8);e[0x4ba]=(u8)ex;
                e[0x4da]=(u8)(ey>>16);e[0x4fa]=(u8)(ey>>8);e[0x51a]=(u8)ey;
                expected_ram[0x30]=(vy&0x8000)?255:0;
            }
            ss2_obj_motion_native(o);
            if(o[0x47a]!=(u8)(ex>>16)||o[0x49a]!=(u8)(ex>>8)||o[0x4ba]!=(u8)ex||
               o[0x4da]!=(u8)(ey>>16)||o[0x4fa]!=(u8)(ey>>8)||o[0x51a]!=(u8)ey||
               ss2_ram[0x30]!=((vy&0x8000)?255:0))motion_failures++;
            if(!(n&255))for(i=0;i<8192;i++)if(expected_ram[i]!=ss2_ram[i])probe_guard_failures++;
        }
        if(!(n&127))SYS_doVBlankProcess();
    }
    probe_done=0x600d;
    while(TRUE)SYS_doVBlankProcess();
}
