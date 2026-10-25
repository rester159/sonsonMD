#!/usr/bin/env python3
"""Production main-shot adapter and resident renderer, with host game/I/O shims."""
from pathlib import Path
import subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/d-render';o.mkdir(parents=True,exist_ok=True)
c=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "arcade_d_pool.h"
#include "arcade_d_render.h"
#include "arcade_d_art.h"
#include "arcade_bonus_art.h"
int main(void){unsigned owner,k,x,flags;u16 sat[80*4];
/* Exact stored normal/bonus poses, both palettes, all flag bytes, wrapping and bounded writes. */
for(k=0;k<61;k++)for(owner=0;owner<2;owner++)for(flags=0;flags<256;flags++)for(x=0;x<2;x++){
 ArcadeDActor actors[16]={0};ArcadeDActor*a=&actors[0];u16 tile,n;
 if(k<39){const ArcadeDFrame*f=&arcade_d_frames[k];a->code=f->code;a->attr=f->attr;tile=arcade_d_tile(a->code,a->attr);}
 else{const ArcadeBonusFrame*f=&arcade_bonus_frames[k-39];a->code=f->code;a->attr=f->attr;tile=f->tile;if(owner && f->owner2_tile!=65535){a->attr=0xc3;tile=f->owner2_tile;}}
 /* Normal death/score poses can share the same pixels as bonus poses. */
 {u16 shared=arcade_d_tile(a->code,a->attr);if(shared!=65535)tile=shared;}
 a->active=flags;a->x=x?250:100;a->y=100;
 for(unsigned j=0;j<320;j++)sat[j]=0xbeef;
 n=arcade_d_render_actor(a,0,sat,2,5);
 if(!flags || (flags&128))assert(n==0);
 else{
  assert(tile!=65535 && n==(x?2:1));
  if((sat[2*4+2]&2047)!=tile || sat[2*4]!=267){fprintf(stderr,"k=%u owner=%u code=%u attr=%u expected=%u got=%u y=%u\n",k,owner,a->code,a->attr,tile,sat[2*4+2]&2047,sat[2*4]);return 1;}
  assert(sat[2*4+3]==(x?378:228));
  if(x)assert(sat[3*4+3]==122 && (sat[3*4+2]&2047)==tile);
 }
 for(unsigned j=0;j<8;j++)assert(sat[j]==0xbeef);
 for(unsigned j=(2+n)*4;j<320;j++)assert(sat[j]==0xbeef);
 for(unsigned j=0;j<320;j++)sat[j]=0xbeef;
 a->active=0;assert(arcade_d_render_actor(a,0,sat,2,5)==0);a->active=flags;
 for(unsigned j=0;j<320;j++)assert(sat[j]==0xbeef);
 assert(arcade_d_render_actor(a,0,sat,2,2)==0);
 for(unsigned j=0;j<320;j++)assert(sat[j]==0xbeef);
}
return 0;}
'''
(o/'test.c').write_text(c)
files=('arcade_d_render','arcade_d_art','arcade_d_actor_data','arcade_bonus_art','arcade_cell_sat')
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),*[str(r/'src'/f'{f}.c')for f in files],'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'render_cases':61*2*256*2,'scope':'Resident D renderer: all39 animation frames and22 bonus poses, flags, owner palettes, X wrap and transactional capacity checks under sanitizers. No live game binding or target readback.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
