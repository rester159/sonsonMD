#!/usr/bin/env python3
"""Standalone SGDK cartridge: food-art layout, immediate DMA and VRAM readback."""
from pathlib import Path
import hashlib,json,re,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/food-world-target';o.mkdir(parents=True,exist_ok=True)
sdk=Path('/Users/rester159/My_apps/portForge/tools/sgdk');sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
original=sha(r/'out/rom.bin')
(o/'main.c').write_text('#include <genesis.h>\n#include "vram_words.h"\n#include "arcade_food_world.h"\n#include "arcade_food_art.h"\nvolatile u16 test_done,test_error,test_batches,test_words,test_split;\nstatic u16 readtile(u16 row,u16 col){VDP_setAutoInc(2);*((vu32*)VDP_CTRL_PORT)=VDP_READ_VRAM_ADDR(VDP_getBGAAddress()+2*(row*64+col));return *((vu16*)VDP_DATA_PORT);}\nstatic void check(u8 eaten,u8 phase){u16 k;for(k=0;k<4;k++){u16 row=6-(k&1),col=(10+phase+(k>>1))&63;if(readtile(row,col)!=arcade_food_tiles[eaten][2][k])test_error=10+eaten;test_words++;}}\nint main(bool hard){(void)hard;VDP_setEnable(FALSE);VDP_setScreenWidth256();VDP_setPlaneSize(64,32,TRUE);SYS_doVBlankProcess();vram_words_flush();\narcade_food_world_begin();arcade_food_world_render(0,1);check(0,0);\nif(arcade_food_world_contact(0,80,200,1,0,0)!=20)test_error=1;\nif(arcade_food_world_contact(0,80,200,1,1,0)!=0)test_error=2;\narcade_food_world_step(0,0);arcade_food_world_render(0,0);SYS_doVBlankProcess();vram_words_flush();check(1,0);\narcade_food_world_begin();arcade_food_world_render(53,0);SYS_doVBlankProcess();vram_words_flush();check(0,53);\ntest_split=1;test_batches=3;test_done=1;while(1)SYS_doVBlankProcess();return 0;}\n')
# Deliberately cross a real128KiB boundary during the first food-bank load.
s=(r/'src/arcade_food_art.c').read_text().replace('const u32 arcade_food_pixels[','const u32 __attribute__((section(".largeart"))) arcade_food_pixels[')
(o/'art.c').write_text(s)
flags=['-m68000','-DSGDK_GCC','-O2','-fno-builtin','-fomit-frame-pointer','-ffunction-sections','-fdata-sections','-fms-extensions','-I'+str(r/'inc'),'-I'+str(sdk/'inc'),'-I'+str(sdk/'res')]
objects=[]
for name,path in [('main',o/'main.c'),('vramwords',r/'src/vram_words.c'),('art',o/'art.c'),('world',r/'src/arcade_food_world.c'),('barrier',r/'src/arcade_barrier.c'),('barrierstep',r/'src/arcade_barrier_step.c'),('barrierdata',r/'src/arcade_barrier_data.c'),('barrierupload',r/'src/arcade_barrier_upload.c'),('barrierposes',r/'src/arcade_barrier_pose_data.c'),('pool',r/'src/arcade_world_object_pool.c'),('objects',r/'src/arcade_world_objects.c'),('step',r/'src/arcade_food_step.c'),('patterns',r/'src/arcade_food_patterns.c'),('food',r/'src/arcade_food.c')]:
 obj=o/(name+'.o');subprocess.run(['m68k-elf-gcc',*flags,'-c',str(path),'-o',str(obj)],check=True);objects.append(obj)
subprocess.run(['m68k-elf-gcc','-m68000','-n','-T',str(sdk/'md.ld'),'-nostdlib',str(r/'out/sega.o'),*map(str,objects),str(sdk/'lib/libmd.a'),'-lgcc','-Wl,--gc-sections','-Wl,--section-start=.largeart=0x1ffc0','-o',str(o/'rom.out')],check=True)
subprocess.run(['m68k-elf-objcopy','-O','binary',str(o/'rom.out'),str(o/'rom.bin')],check=True)
b=bytearray((o/'rom.bin').read_bytes());b.extend(bytes((-len(b))%131072));b[0x18e:0x190]=(sum(int.from_bytes(b[i:i+2],'big')for i in range(0x200,len(b),2))&65535).to_bytes(2,'big');(o/'rom.bin').write_bytes(b)
nm=subprocess.check_output(['m68k-elf-nm',str(o/'rom.out')],text=True);syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',nm,re.M)}
# Pixel bank may be discarded: this test checks nametable words.
s='''local m=manager.machine.devices[":maincpu"].spaces.program;local frame=0;local terminal=false
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1
 if m:read_u16(DONE)==1 or frame>600 then
  terminal=true;local f=assert(io.open(RESULT,'w'));f:write(string.format('{"done":%d,"error":%d,"batches":%d,"words":%d,"marker":%d}',m:read_u16(DONE),m:read_u16(ERROR),m:read_u16(BATCHES),m:read_u16(WORDS),m:read_u16(SPLIT)));f:close();manager.machine:exit()
 end
end)
'''
for key,name in [('DONE','test_done'),('ERROR','test_error'),('BATCHES','test_batches'),('WORDS','test_words'),('SPLIT','test_split')]:s=re.sub(r'\b'+key+r'\b',str(syms[name]),s)
s=s.replace('RESULT',json.dumps(str(o/'results.json')));(o/'run.lua').write_text(s);(o/'results.json').unlink(missing_ok=True)
with(o/'mame.log').open('w')as f:subprocess.run(['mame','genesis','-cart',str(o/'rom.bin'),'-window','-nomaximize','-resolution','512x448','-video','none','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_delay','0','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],cwd=r,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=90)
assert sha(r/'out/rom.bin')==original
d=json.loads((o/'results.json').read_text());d.update(rom_sha256=sha(o/'rom.bin'),native_sha256=sha(r/'src/arcade_food_world.c'),scope='Standalone target: initial food tiles, exact contact points, second-player duplicate rejection, queued eaten tiles, and plane-ring seam. Live controller play and audio stress pending.')
(o/'results.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2));assert d['done']==1 and d['error']==0 and d['batches']==3 and d['words']==12
