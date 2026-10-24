#!/usr/bin/env python3
"""Windowless 68000 prelude scene smoke; isolated from production output."""
from pathlib import Path
import hashlib,json,re,subprocess
from genplus_probe import probe
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/prelude-display';o.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
original=sha(r/'out/rom.bin');sdk=Path('/Users/rester159/My_apps/portForge/tools/sgdk')
(o/'main.c').write_text('''#include <genesis.h>
#include "arcade_prelude_display.h"
#include "arcade_prelude_terrain.h"
volatile u16 test_done,test_error,test_batches,test_words,test_split;
int main(bool hard){
 static ArcadePrelude scene;static ArcadePreludeRewards rewards;
 ArcadePreludeTerrain terrain={0};ArcadeBonusAward events[2];u8 out[2]={0};(void)hard;
 scene.animation[0].attr=0xc0;scene.animation[1].attr=0xc1; /* Same cold palette setup as game_init. */
 arcade_players_course_begin(scene.players,scene.animation,0,1,out);
 arcade_prelude_players_begin(&scene,arcade_prelude_floor,&terrain,0);
 arcade_prelude_decor_begin(&scene);arcade_prelude_display_begin();
 if(!arcade_prelude_display_publish(&scene))test_error=3;
 for(u16 tick=0;tick<640;tick++){
  SYS_doVBlankProcess();
  if(tick%120==0)arcade_prelude_display_hud(543210,tick/120,tick>=480,tick>=240);
  if(!arcade_prelude_step(&scene,tick,arcade_prelude_floor,&terrain,0))test_error=1;
  (void)arcade_prelude_collect(&scene,&rewards,events);
  if(!arcade_prelude_display_publish(&scene))test_error=2;
  if(test_error)break;
  test_words++;
 }
 test_batches=1;test_split=1;test_done=1;
 while(1)SYS_doVBlankProcess();return 0;
}
''')
sources=['arcade_prelude','arcade_prelude_render','arcade_prelude_display','arcade_prelude_hud','arcade_prelude_hud_art','arcade_prelude_terrain','arcade_prelude_terrain_data','arcade_player','arcade_player_course','arcade_attract','arcade_attract_data','arcade_pickup','arcade_rewards','arcade_difficulty','arcade_bonus_art','arcade_cell_sat','arcade_life_art','ss_title','ss_anim','ss_gfx']
flags=['-m68000','-DSGDK_GCC','-O2','-fno-builtin','-fomit-frame-pointer','-ffunction-sections','-fdata-sections','-fms-extensions','-I'+str(r/'inc'),'-I'+str(sdk/'inc'),'-I'+str(sdk/'res')]
objects=[]
for path in [o/'main.c']+[r/'src'/(n+'.c')for n in sources]:
 obj=o/(path.stem+'.o');subprocess.run(['m68k-elf-gcc',*flags,'-c',str(path),'-o',str(obj)],check=True);objects.append(obj)
subprocess.run(['m68k-elf-gcc','-m68000','-n','-T',str(sdk/'md.ld'),'-nostdlib',str(r/'out/sega.o'),*map(str,objects),str(sdk/'lib/libmd.a'),'-lgcc','-Wl,--gc-sections','-o',str(o/'rom.out')],check=True)
subprocess.run(['m68k-elf-objcopy','-O','binary',str(o/'rom.out'),str(o/'rom.bin')],check=True)
b=bytearray((o/'rom.bin').read_bytes());b.extend(bytes((-len(b))%131072));b[0x18e:0x190]=(sum(int.from_bytes(b[i:i+2],'big')for i in range(0x200,len(b),2))&65535).to_bytes(2,'big');(o/'rom.bin').write_bytes(b)
nm=subprocess.check_output(['m68k-elf-nm',str(o/'rom.out')],text=True)
syms={n:int(v,16) for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',nm,re.M)}
d=probe(o/'rom.bin',syms,o,800,capture_frames=(40,120,360,600))
for frame in (40,120,360,600):
 assert (o/f'frame-{frame}.ppm').exists()
d['captures']={str(frame):sha(o/f'frame-{frame}.ppm')for frame in (40,120,360,600)}
assert sha(r/'out/rom.bin')==original
assert d['done']==1 and d['error']==0 and d['words']==640,d
d.update(rom_sha256=sha(o/'rom.bin'),production_rom_unchanged=original,scope='Isolated68000 scene: cold resources, source terrain ring0, scripted players, pickups/contact and640SAT publications. Captured title background reused. No image/VRAM comparison, source full-scene trace, sound or production title binding.')
(o/'report.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2))
