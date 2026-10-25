#!/usr/bin/env python3
"""Source-pinned prelude setup and bounded native scene checks."""
from pathlib import Path
import hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/prelude-setup';o.mkdir(parents=True,exist_ok=True)
rom=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(rom).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
pins={0x93d9:'cc001ced15c688e718cc00d0ed35c688e738',0x943f:'cc0200ed11cc005ced15c6b4e718cc0200ed31cc0090ed35c6b4e738',0xb23d:'e6132a03bd9617',0x9463:'bd9a2cbda98abdb19a'}
for a,h in pins.items():assert rom[a-0x4000:a-0x4000+len(bytes.fromhex(h))]==bytes.fromhex(h)
# First source script pair commands inward movement for64 updates.
assert rom[0x5bb6:0x5bb8]==bytes([8,63]) and rom[0x5bc4:0x5bc6]==bytes([4,63])
(o/'test.c').write_text('''#include <assert.h>
#include <string.h>
#include "arcade_prelude.h"
static s16 floor_probe(u16 x,u8 y,void *ctx){(void)x;(void)y;(void)ctx;return 1;}
int main(void){
 for(unsigned fraction=0;fraction<256;fraction++){
  ArcadePrelude s={0};ArcadeFirePose fire[2];
  for(unsigned i=0;i<2;i++){
   s.players[i].xf=fraction;s.players[i].yf=255-fraction;
   s.players[i].timer=37;s.players[i].stamp=91;s.players[i].facing=1;
   s.animation[i].owner=i;
  }
  arcade_prelude_players_begin(&s,floor_probe,0,fire);
  for(unsigned i=0;i<2;i++){
   assert(s.players[i].x==(i?208:28) && s.players[i].y==136);
   assert(s.players[i].xf==fraction && s.players[i].yf==255-fraction);
   assert(s.players[i].active==1 && s.players[i].state==1 && s.players[i].phase==0);
   assert(s.players[i].timer==37 && s.players[i].stamp==91);
   assert(s.animation[i].code==(i?0x48:0x40));
  }
  arcade_prelude_decor_begin(&s);
  ArcadePreludeDecor decor[2];memcpy(decor,s.decor,sizeof(decor));
  for(unsigned i=0;i<2;i++){
   assert(s.decor[i].active==1 && s.decor[i].kind==8+i && s.decor[i].y==180);
   assert(s.decor[i].x==(i?144:92));assert(s.decor[i].code==arcade_bonus_frames[arcade_bonus_roots[8+i]].code);
  }
  for(unsigned tick=1;tick<=40;tick++){
   assert(arcade_prelude_step(&s,tick,floor_probe,0,fire));
   assert(s.players[0].x==28+tick && s.players[1].x==208-tick);
   assert(s.players[0].xf==fraction && s.players[1].xf==fraction);
   assert(s.players[0].y==136 && s.players[1].y==136);
   assert(!memcmp(decor,s.decor,sizeof(decor)));
  }
  s.input.delay[0]=0;s.input.cursor[0]=355;
  ArcadePrelude before=s;
  assert(!arcade_prelude_step(&s,0,floor_probe,0,fire));assert(!memcmp(&s,&before,sizeof(s)));
 }
 return 0;
}
''')
sources=['arcade_prelude.c','arcade_attract.c','arcade_attract_data.c','arcade_player.c','arcade_bonus_art.c','arcade_pickup.c','arcade_rewards.c','arcade_difficulty.c']
subprocess.run(['cc','-std=c99','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),*[str(r/'src'/f)for f in sources],'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
sdk=Path('/Users/rester159/My_apps/portForge/tools/sgdk')
subprocess.run(['m68k-elf-gcc','-m68000','-DSGDK_GCC','-fno-builtin','-O2','-Wall','-Wextra','-Werror','-I'+str(r/'inc'),'-I'+str(sdk/'inc'),'-I'+str(sdk/'res'),'-c',str(r/'src/arcade_prelude.c'),'-o',str(o/'prelude.o')],check=True)
report={'passed':True,'fraction_cases':256,'scripted_actor_updates':10240,'source_signatures':len(pins),
 'source_sha256':hashlib.sha256((r/'src/arcade_prelude.c').read_bytes()).hexdigest(),
 'scope':'Source-pinned player/decor positions and fixed positive-kind poses; initialized actor/animation fields, all fractional bytes, first40 script updates with synthetic supporting floor, atomic script bounds failure. ASan/UBSan and strict68000compile. No original-CPU full actor trace, real terrain, shots/collisions or live title integration.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
