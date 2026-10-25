#!/usr/bin/env python3
"""Original bitmap and A847 probe semantics, all prelude ring phases."""
from pathlib import Path
import hashlib,json,re,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/prelude-terrain';o.mkdir(parents=True,exist_ok=True)
rom=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(rom).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
assert rom[0x507d:0x5094]==bytes.fromhex('9e8e108e0100c620d7c8ec81eda1ec81eda10ac826f439')
assert rom[0x6878:0x6888]==bytes.fromhex('e618c40fc002c10e2206c005501a0139')
bitmap=rom[0x425b:0x42db]
source=(r/'src/arcade_prelude_terrain_data.c').read_text()
assert bytes(int(x,16)for x in re.findall('0x([0-9a-f]{2})',source))==bitmap
(o/'test.c').write_text('''#include <assert.h>
#include "arcade_prelude_terrain.h"
#include "arcade_prelude.h"
static const u8 original[128]={'''+','.join(map(str,bitmap))+'''};
int main(void){
 for(unsigned ring=0;ring<32;ring++){
  ArcadePreludeTerrain terrain={ring};
  for(unsigned x=0;x<256;x++)for(unsigned y=0;y<256;y++){
   unsigned column=((x+8)/8+ring)%32;
   unsigned bit=y/8;unsigned low=y%16;
   int want=(original[column*4+bit/8]&(128>>(bit%8))) && low>=2 ? 7-(int)low:ARCADE_NO_FLOOR;
   assert(arcade_prelude_floor(x,y,&terrain)==want);
   assert(arcade_prelude_floor(x+0xff00,y,&terrain)==want);
  }
  ArcadePrelude s={0};u8 out[2]={0};
  arcade_players_course_begin(s.players,s.animation,0,1,out);
  arcade_prelude_players_begin(&s,arcade_prelude_floor,&terrain,0);
  arcade_prelude_decor_begin(&s);
  for(unsigned tick=0;tick<640;tick++){
   assert(arcade_prelude_step(&s,tick,arcade_prelude_floor,&terrain,0));
   for(unsigned i=0;i<2;i++)assert(s.players[i].state<=5);
  }
 }
 return 0;
}
''')
sources=['arcade_prelude_terrain.c','arcade_prelude_terrain_data.c','arcade_prelude.c','arcade_player.c','arcade_player_course.c','arcade_attract.c','arcade_attract_data.c','arcade_bonus_art.c','arcade_pickup.c','arcade_rewards.c','arcade_difficulty.c']
subprocess.run(['cc','-std=c99','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),*[str(r/'src'/f)for f in sources],'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
sdk=Path('/Users/rester159/My_apps/portForge/tools/sgdk')
subprocess.run(['m68k-elf-gcc','-m68000','-DSGDK_GCC','-fno-builtin','-O2','-Wall','-Wextra','-Werror','-I'+str(r/'inc'),'-I'+str(sdk/'inc'),'-I'+str(sdk/'res'),'-c',str(r/'src/arcade_prelude_terrain.c'),'-o',str(o/'terrain.o')],check=True)
report={'passed':True,'probe_checks':32*256*256*2,'ring_phases':32,'scripted_updates':32*640,
 'bitmap_sha256':hashlib.sha256(bitmap).hexdigest(),
 'scope':'Original first-segment bitmap; all x/y bytes and ring phases, low/high word X alias, A847 signed push-out/no-floor semantics; native prelude640update sanitizer smoke for each phase. Does not establish original ring lifecycle, full actor trace, collisions/shots or live scene integration.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
