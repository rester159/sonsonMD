#!/usr/bin/env python3
"""Prelude SAT positions, source order, wraps and atomic capacity checks."""
from pathlib import Path
import hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/prelude-render';o.mkdir(parents=True,exist_ok=True)
rom=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert rom[0xbf4a:0xbf50]==bytes.fromhex('e610271b2b19')
(o/'test.c').write_text('''#include <assert.h>
#include <string.h>
#include "arcade_prelude_render.h"
static unsigned expected(u16 *out,unsigned first,unsigned x,unsigned y,unsigned tile,unsigned attr,const ArcadeSpriteView *v){
 unsigned n=0;int sx=(int)(u8)x-v->x,sy=(int)(u8)(-y-16)-v->y;
 for(unsigned copy=0;copy<3;copy++){
  int px=sx-(copy==1?256:0),py=sy-(copy==2?256:0);
  if(px<=-16||px>=v->width||py<=-16||py>=v->height)continue;
  unsigned at=(first+n)*4;
  out[at]=(128+py)&1023;out[at+1]=0x500|first+n+1;
  out[at+2]=0x2000|tile|((attr&64)?0:0x800)|((attr&128)?0:0x1000);
  out[at+3]=(128+px)&511;n++;
 }
 return n;
}
int main(void){
 const ArcadeSpriteView views[3]={{0,1,256,224},{0,0,256,224},{8,24,240,192}};
 for(unsigned x=0;x<256;x+=7)for(unsigned y=0;y<256;y+=11)
 for(unsigned mask=0;mask<16;mask++)for(unsigned v=0;v<3;v++){
  ArcadePrelude s={0};u16 got[80],want[80];unsigned count=0,first=2;
  for(unsigned i=0;i<2;i++){
   s.players[i].active=(mask>>i)&1;s.players[i].x=(x+i*60)&255;s.players[i].y=(y+i*40)&255;
   s.animation[i].code=i?0x48:0x40;s.animation[i].attr=(x&192)|i;
   if(s.players[i].active)count+=expected(want,first+count,s.players[i].x,s.players[i].y,i?322:290,s.animation[i].attr,&views[v]);
  }
  for(unsigned i=0;i<2;i++){
   ArcadePreludeDecor *d=&s.decor[i];d->active=(mask>>(i+2))&1;d->x=92+i*52;d->y=180;d->code=arcade_bonus_frames[arcade_bonus_roots[8+i]].code;d->attr=arcade_bonus_frames[arcade_bonus_roots[8+i]].attr;
   const ArcadeBonusFrame *f=&arcade_bonus_frames[arcade_bonus_roots[8+i]];
   if(d->active)count+=expected(want,first+count,d->x,d->y,f->tile,f->attr,&views[v]);
  }
  for(unsigned limit=first;limit<=first+count+1;limit++){
   for(unsigned i=0;i<80;i++)got[i]=0xdead;
   unsigned result=arcade_prelude_sat(&s,&views[v],got,first,limit);
   if(limit<first+count){assert(result==65535);for(unsigned i=0;i<80;i++)assert(got[i]==0xdead);}
   else{
    assert(result==count);
    assert(!memcmp(got+first*4,want+first*4,count*8));
    for(unsigned i=0;i<first*4;i++)assert(got[i]==0xdead);
    for(unsigned i=(first+count)*4;i<80;i++)assert(got[i]==0xdead);
   }
  }
 }
 ArcadePrelude bad={0};u16 sat[48];memset(sat,0x55,sizeof(sat));bad.decor[0].active=1;bad.decor[0].code=255;
 assert(arcade_prelude_sat(&bad,&views[0],sat,0,12)==65535);
 for(unsigned i=0;i<48;i++)assert(sat[i]==0x5555);
 for(unsigned active=128;active<256;active++){
  bad.decor[0].active=active;bad.players[0].active=active;
  bad.animation[0].code=255; /* Hidden actors must not resolve invalid art. */
  assert(arcade_prelude_sat(&bad,&views[0],sat,0,12)==0);
  for(unsigned i=0;i<48;i++)assert(sat[i]==0x5555);
 }
 return 0;
}
''')
sources=['arcade_prelude_render.c','arcade_cell_sat.c','arcade_life_art.c','arcade_bonus_art.c']
subprocess.run(['cc','-std=c99','-O1','-Wall','-Wextra','-Werror','-Wno-parentheses','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),*[str(r/'src'/f)for f in sources],'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
sdk=Path('/Users/rester159/My_apps/portForge/tools/sgdk')
subprocess.run(['m68k-elf-gcc','-m68000','-DSGDK_GCC','-fno-builtin','-O2','-Wall','-Wextra','-Werror','-I'+str(r/'inc'),'-I'+str(sdk/'inc'),'-I'+str(sdk/'res'),'-c',str(r/'src/arcade_prelude_render.c'),'-o',str(o/'render.o')],check=True)
report={'passed':True,'scene_cases':37*24*16*3,'source_sha256':hashlib.sha256((r/'src/arcade_prelude_render.c').read_bytes()).hexdigest(),
 'scope':'Host SAT composition: player/decor order, attributes, coordinates, independent wrap copies, three viewports, all activity masks and exact/insufficient capacities with canaries. ASan/UBSan and strict68000compile. Reuses existing verified art/converter; no live VDP or original screenshot comparison.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
