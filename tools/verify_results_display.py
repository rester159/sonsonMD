#!/usr/bin/env python3
"""Production display owner against a constrained copied-DMA queue, offline."""
from pathlib import Path
import json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/results-display'
o.mkdir(parents=True,exist_ok=True)
(o/'genesis.h').write_text('''#pragma once
#include <stdint.h>
typedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;
typedef int16_t s16;typedef int32_t s32;
#define DMA 0
#define DMA_VRAM 0
void VDP_loadTileData(const u32*,u16,u16,u16);
u16 DMA_getQueueSize(void);u16 DMA_getMaxQueueSize(void);
u16 DMA_canQueue(u16,u16);u16 VDP_getBGAAddress(void);
u16 DMA_copyAndQueueDma(u16,void*,u16,u16,u16);
''')
(o/'test.c').write_text('''#include <assert.h>
#include <string.h>
#include <genesis.h>
#include "arcade_results_display.h"
#include "arcade_results_art.h"
#include "arcade_results_decor.h"
static u16 queued,allowed=1,fail,loads;
static u16 destination[2],copied[2][32],vram[2048],seen[32];
void VDP_loadTileData(const u32*p,u16 b,u16 n,u16 m){
 (void)p;(void)m;
 if(loads==0)assert(b==136 && n==ARCADE_RESULTS_ART_TILES);
 if(loads==1)assert(b==136+ARCADE_RESULTS_ART_TILES && n==24);
 if(loads==2)assert(b==136 && n==132);loads++;
}
u16 DMA_getQueueSize(void){return queued;}
u16 DMA_getMaxQueueSize(void){return 2;}
u16 DMA_canQueue(u16 t,u16 n){(void)t;assert(n==32);return allowed;}
u16 VDP_getBGAAddress(void){return 0xe000;}
u16 DMA_copyAndQueueDma(u16 t,void*p,u16 a,u16 n,u16 step){
 (void)t;assert(n==32 && step==2);
 if(fail || queued==2)return 0;
 destination[queued]=a;memcpy(copied[queued],p,64);queued++;return 1;
}
static void flush(void){u16 i;for(i=0;i<queued;i++){
 u16 offset=(destination[i]-0xe000)/2;assert(offset%64==0);
 memcpy(vram+offset,copied[i],64);seen[offset/64]++;
}queued=0;}
int main(void){
 u16 row,col,sat[24],old[64];ArcadeResultsEvent e={0};
 memset(vram,0x5a,sizeof(vram));arcade_results_display_begin();
 allowed=0;arcade_results_display_background();assert(!queued && arcade_results_display_pending());
 allowed=1;fail=1;arcade_results_display_background();assert(!queued && arcade_results_display_pending());fail=0;
 while(arcade_results_display_pending()){arcade_results_display_background();flush();}
 for(row=0;row<32;row++)for(col=0;col<64;col++){
  assert(vram[row*64+col]==((row>=5 && row<31 && col<32)?136+arcade_results_digits[11]:0x5a5a));
  assert(seen[row]==(row>=5 && row<31));
 }
 e.flags=ARCADE_RESULTS_BOARD;e.number=1;arcade_results_display_event(&e);
 arcade_results_display_background();assert(queued==1 && destination[0]==0xe000+7*128);
 memcpy(old,copied,sizeof(copied));e.flags=ARCADE_RESULTS_CLEAR;arcade_results_display_event(&e);
 assert(!memcmp(old,copied,sizeof(copied)));flush();
 while(arcade_results_display_pending()){arcade_results_display_background();flush();}
 memset(sat,0x5a,sizeof(sat));assert(arcade_results_display_sat(sat,0,3)==0);
 e.flags=ARCADE_RESULTS_DECOR;arcade_results_display_event(&e);
 assert(arcade_results_display_sat(sat,0,2)==65535 && sat[0]==0x5a5a);
 assert(arcade_results_display_sat(sat,1,4)==3);
 for(row=0;row<3;row++){
  assert(sat[(row+1)*4]==335);
  assert(sat[(row+1)*4+1]==(0x500|row+2));
  assert(sat[(row+1)*4+2]==(0x2000|136+ARCADE_RESULTS_ART_TILES+row*8));
  assert(sat[(row+1)*4+3]==128+arcade_results_positions[row].x);
 }
 arcade_results_display_restore();assert(loads==3 && !arcade_results_display_pending());
 assert(arcade_results_display_sat(sat,0,3)==0);return 0;
}
''')
sources=['arcade_results_display.c','arcade_results_map.c','arcade_results_art.c','arcade_results_decor.c','arcade_results_decor_data.c','arcade_bonus_art.c','arcade_cell_sat.c']
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-Wno-parentheses','-DSGDK_GCC','-I'+str(o),'-I'+str(r/'inc'),str(o/'test.c')]+[str(r/'src'/s)for s in sources]+['-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'passed':True,'clear_plane_cells':2048,'scope':'Host production-display check: shared arena load/restore, two-entry DMA queue, admission/copy-failure retry, immutable queued copies across map mutation, exact plane stride, dirty row selection and decoration SAT placement/capacity. Live scene and target DMA execution pending.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
