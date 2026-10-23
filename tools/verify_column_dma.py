#!/usr/bin/env python3
"""Actual paired-column helper: addresses, art identity and retry boundaries."""
from pathlib import Path
import re,subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/column-dma/host';o.mkdir(exist_ok=True)
s=(r/'out/parity-audit/column-dma/src/arcade_food_world.c').read_text()
arrays=s[s.index('static const u16 barrier_maps'):s.index('static u16 screen;')]
helper=s[s.index('static u8 food_column_dma('):s.index('void arcade_food_world_render(')]
c='''#include <assert.h>
#include "arcade_types.h"
#include "arcade_food_art.h"
#define DMA_VRAM 0
static u16 expected_addr[2],expected_tile[2],seen,attempt,fail_at;
static u16 VDP_getBGAAddress(void){return 0xc000;}
static u8 DMA_queueDma(u8 loc,void *src,u16 address,u16 words,u16 step){
 assert(loc==0);if(attempt++==fail_at)return 0;
 for(u16 k=0;k<words;k++,address+=step){
  u16 j;for(j=0;j<2;j++)if(expected_addr[j]==address)break;
  assert(j<2);assert(((u16*)src)[k]==expected_tile[j]);seen|=1u<<j;
 }
 return 1;
}
static void DMA_doDma(u8 loc,void *src,u16 address,u16 words,u16 step){assert(DMA_queueDma(loc,src,address,words,step));}
'''+arrays+helper+'''
int main(void){unsigned a,row,col,side,cold,fail;
 for(a=0;a<52;a++)for(side=0;side<2;side++){
  const u16 *pair=a<40?food_columns[a/20][a%20]+side*2:barrier_maps[a-40]+side*2;
  expected_tile[0]=a<40?arcade_food_tiles[a/20][a%20][side*2]:1984+(a-40)*4+side*2;
  expected_tile[1]=a<40?arcade_food_tiles[a/20][a%20][side*2+1]:expected_tile[0]+1;
  for(row=0;row<32;row++)for(col=0;col<64;col++)for(cold=0;cold<2;cold++)for(fail=0;fail<3;fail++){
   expected_addr[0]=0xc000+2*(row*64+col);expected_addr[1]=0xc000+2*(((row+31)&31)*64+col);
   seen=attempt=0;fail_at=cold?65535:fail;
   u8 ok=food_column_dma(pair,row,col,cold);if(ok)assert(seen==3);
   fail_at=65535;assert(food_column_dma(pair,row,col,cold));assert(seen==3);
  }
 }
 return 0;
}
''';(o/'test.c').write_text(c)
subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),str(r/'src/arcade_food_art.c'),'-o',str(o/'test')],check=True);subprocess.run([str(o/'test')],check=True)
report={'column_retry_cases':52*2*32*64*2*3,'scope':'Actual paired DMA helper and tables vs original tile identities. Every plane row/column, both columns, all food/eaten/barrier maps, immediate/queued paths, failure/retry. Host transfer shim, not target DMA or full-scene pixel proof.'};(o/'report.json').write_text(json.dumps(report,indent=2));print(report)
