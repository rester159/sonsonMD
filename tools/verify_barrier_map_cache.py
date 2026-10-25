#!/usr/bin/env python3
"""Exercise an experimental cache against a complete plane, including partial DMA."""
from pathlib import Path
import subprocess,json,hashlib,argparse
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args();s=a.source.read_text()
o=r/'out/parity-audit/barrier-map-cache/host';o.mkdir(parents=True,exist_ok=True)
cache=s[s.index('/* Direct-mapped memo'):s.index('static u16 screen;')]
helper=s[s.index('static u8 food_column_dma('):s.index('void arcade_food_world_render(')]
body=s[s.index('            u16 pair=slot*2+(k>>1);'):s.index('\n        }\n        if(k==4)barrier_writes')]
body=body.replace('continue;','return 1;').replace('break;','return 0;')
arrays=s[s.index('static const u16 barrier_maps'):s.index('#include "arcade_food_columns.h"')]
c='''#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;
#define DMA_VRAM 0
static u16 plane[2048];static int fail_at,attempt;
static u16 VDP_getBGAAddress(void){return 0xe000;}
static u8 DMA_queueDma(u8 loc,void *source,u16 address,u16 words,u16 step){
 (void)loc;if(attempt++==fail_at)return 0;
 for(u16 k=0;k<words;k++,address+=step)plane[(address-0xe000)/2]=((u16*)source)[k];return 1;
}
static void DMA_doDma(u8 loc,void *source,u16 address,u16 words,u16 step){assert(DMA_queueDma(loc,source,address,words,step));}
'''+arrays+cache+helper+'''
static u8 write_pair(u16 row,u16 col,u16 slot,u16 k){
'''+body+'''
 return 1;
}
static u32 rng=4519;static u32 random_word(void){rng=rng*1664525u+1013904223u;return rng;}
int main(void){unsigned hits=0;
 arcade_food_world_invalidate_map();
 for(unsigned i=0;i<1000000;i++){
  u16 row=(random_word()>>16)&31,col=(random_word()>>16)&63;
  u16 slot=(random_word()>>16)%12,k=((random_word()>>16)&1)*2;
  if(i%13==0){arcade_food_world_invalidate_map();plane[random_word()%2048]=65535;}
  /* Force epoch rollover repeatedly, with previous entries still populated. */
  if(i%100003==0){map_epoch=65535;arcade_food_world_invalidate_map();}
  attempt=0;fail_at=(random_word()>>16)%4-1;
  u8 ok=write_pair(row,col,slot,k);
  if(ok){assert(plane[row*64+col]==barrier_maps[slot][k+1]);assert(plane[((row+31)&31)*64+col]==barrier_maps[slot][k]);}
  attempt=0;fail_at=-1;assert(write_pair(row,col,slot,k));
  attempt=0;assert(write_pair(row,col,slot,k));assert(attempt==0);hits++;
  /* A neighbouring pair shares exactly one cell, including rows 0/31. */
  assert(write_pair((row+1)&31,col,(slot+1)%12,k));
  assert(write_pair(row,col,slot,k));
  assert(plane[row*64+col]==barrier_maps[slot][k+1]);assert(plane[((row+31)&31)*64+col]==barrier_maps[slot][k]);
 }
 printf("%u repeated hits verified\\n",hits);return 0;
}
'''
(o/'test.c').write_text(c)
subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(o/'test.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'source_sha256':hashlib.sha256(s.encode()).hexdigest(),'iterations':1000000,'scope':'Extracted candidate cache and column renderer against full plane storage. Random rows/columns/slot identities, queue failures, repeated hits, overlapping pairs, foreign writes and epoch rollover. Host evidence, not target scene parity.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
