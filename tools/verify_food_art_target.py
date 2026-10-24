#!/usr/bin/env python3
"""Standalone SGDK cartridge: food-art layout, immediate DMA and VRAM readback."""
from pathlib import Path
import hashlib,json,re,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/food-art-target';o.mkdir(parents=True,exist_ok=True)
sdk=Path('/Users/rester159/My_apps/portForge/tools/sgdk');sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
original=sha(r/'out/rom.bin')
(o/'main.c').write_text('#include <genesis.h>\n#include "arcade_food_art.h"\nvolatile u16 test_done,test_error,test_batches,test_words,test_split;\nstatic const u16 guards[4]={280*32-2,288*32,1372*32-2,1440*32};\nint main(bool hard)\n{\n u16 k,j,base,tiles;const u16 *expected=(const u16*)arcade_food_pixels;(void)hard;\n VDP_setEnable(FALSE);VDP_setScreenWidth256();VDP_setPlaneSize(64,64,TRUE);\n SYS_doVBlankProcess();\n if(arcade_food_art_load()){test_error=1;goto done;}\n VDP_setPlaneSize(64,32,TRUE);SYS_doVBlankProcess();VDP_setAutoInc(2);\n test_split=TILE_FONT_INDEX;\n if(test_split!=1440){test_error=2;goto done;}\n for(k=0;k<4;k++){*((vu32*)VDP_CTRL_PORT)=VDP_WRITE_VRAM_ADDR(guards[k]);*((vu16*)VDP_DATA_PORT)=0x1234+k;}\n if(!arcade_food_art_load()){test_error=3;goto done;}\n VDP_setAutoInc(2);\n for(k=0;k<2;k++){\n  base=k?1372:280;tiles=k?68:8;\n  *((vu32*)VDP_CTRL_PORT)=VDP_READ_VRAM_ADDR(base*32);\n  for(j=0;j<tiles*16;j++){if(*((vu16*)VDP_DATA_PORT)!=*expected++){test_error=4;goto done;}test_words++;}\n }\n for(k=0;k<4;k++){\n  *((vu32*)VDP_CTRL_PORT)=VDP_READ_VRAM_ADDR(guards[k]);\n  if(*((vu16*)VDP_DATA_PORT)!=(u16)(0x1234+k)){test_error=5;goto done;}\n }\n test_batches=1;\n done:test_done=1;while(1)SYS_doVBlankProcess();return 0;\n}\n')
# Deliberately cross a real128KiB boundary during the first food-bank load.
s=(r/'src/arcade_food_art.c').read_text().replace('const u32 arcade_food_pixels[','const u32 __attribute__((section(".largeart"))) arcade_food_pixels[')
(o/'art.c').write_text(s)
flags=['-m68000','-DSGDK_GCC','-O2','-fno-builtin','-fomit-frame-pointer','-ffunction-sections','-fdata-sections','-fms-extensions','-I'+str(r/'inc'),'-I'+str(sdk/'inc'),'-I'+str(sdk/'res')]
objects=[]
for name,path in [('main',o/'main.c'),('art',o/'art.c'),('upload',r/'src/arcade_food_art_load.c')]:
 obj=o/(name+'.o');subprocess.run(['m68k-elf-gcc',*flags,'-c',str(path),'-o',str(obj)],check=True);objects.append(obj)
subprocess.run(['m68k-elf-gcc','-m68000','-n','-T',str(sdk/'md.ld'),'-nostdlib',str(r/'out/sega.o'),*map(str,objects),str(sdk/'lib/libmd.a'),'-lgcc','-Wl,--gc-sections','-Wl,--section-start=.largeart=0x1ffc0','-o',str(o/'rom.out')],check=True)
subprocess.run(['m68k-elf-objcopy','-O','binary',str(o/'rom.out'),str(o/'rom.bin')],check=True)
b=bytearray((o/'rom.bin').read_bytes());b.extend(bytes((-len(b))%131072));b[0x18e:0x190]=(sum(int.from_bytes(b[i:i+2],'big')for i in range(0x200,len(b),2))&65535).to_bytes(2,'big');(o/'rom.bin').write_bytes(b)
nm=subprocess.check_output(['m68k-elf-nm',str(o/'rom.out')],text=True);syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',nm,re.M)}
assert syms['arcade_food_pixels']==0x1ffc0
s='''local m=manager.machine.devices[":maincpu"].spaces.program;local frame=0;local terminal=false
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1
 if m:read_u16(DONE)==1 or frame>600 then
  terminal=true;local f=assert(io.open(RESULT,'w'));f:write(string.format('{"done":%d,"error":%d,"batches":%d,"words":%d,"font_base":%d}',m:read_u16(DONE),m:read_u16(ERROR),m:read_u16(BATCHES),m:read_u16(WORDS),m:read_u16(SPLIT)));f:close();manager.machine:exit()
 end
end)
'''
for key,name in [('DONE','test_done'),('ERROR','test_error'),('BATCHES','test_batches'),('WORDS','test_words'),('SPLIT','test_split')]:s=re.sub(r'\b'+key+r'\b',str(syms[name]),s)
s=s.replace('RESULT',json.dumps(str(o/'results.json')));(o/'run.lua').write_text(s);(o/'results.json').unlink(missing_ok=True)
with(o/'mame.log').open('w')as f:subprocess.run(['mame','genesis','-cart',str(o/'rom.bin'),'-window','-nomaximize','-resolution','512x448','-video','none','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_delay','0','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],cwd=r,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=90)
assert sha(r/'out/rom.bin')==original
d=json.loads((o/'results.json').read_text());d.update(rom_sha256=sha(o/'rom.bin'),native_sha256=sha(r/'src/arcade_food_art_load.c'),scope='Standalone target: actual SDK H32/64x32 layout, immediate ROM DMA across128KiB boundary,76 tiles read back and four neighbor guards; incompatible layout rejection. Gameplay and audio stress pending.')
(o/'results.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2));assert d['done']==1 and d['error']==0 and d['batches']==1 and d['words']==1216 and d['font_base']==1440
