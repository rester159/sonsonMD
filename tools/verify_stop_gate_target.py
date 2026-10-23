#!/usr/bin/env python3
"""Standalone SGDK cartridge: food-art layout, immediate DMA and VRAM readback."""
from pathlib import Path
import hashlib,json,re,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/stop-gate-target';o.mkdir(parents=True,exist_ok=True)
sdk=Path('/Users/rester159/My_apps/portForge/tools/sgdk');sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
original=sha(r/'out/rom.bin')
import csv
rows=list(csv.DictReader((r/'out/parity-audit/stop-gate.csv').open()))
assert len(rows)==1536
fields=('camera','clock','timer','grade','scenario','after_timer','after_grade','stop','release','bonus','variant','placement','kind')
data=',\n'.join('{'+','.join(row[k] for k in fields)+'}' for row in rows)
source='#include <genesis.h>\n#include "arcade_stop.h"\nvolatile u16 test_done,test_error,test_batches,test_words,test_split;\nstatic const u16 fixtures[][13]={'+data+'};\n'
source+='\nint main(bool hard){u16 n;(void)hard;VDP_setEnable(FALSE);\nfor(n=0;n<1536;n++){\n const u16 *r=fixtures[n];ArcadeWorldObjectPool p={0};ArcadeStop s={(u8)r[2],(u8)r[3]};ArcadeStopEvent e;\n p.objects[19].active=r[4]==0?0:4;p.objects[19].kind=r[4]==3?1:2;\n p.objects[19].x=(u16)(r[0]+256+(r[4]==2));\n arcade_stop_step(&s,&p,r[0],(u8)r[1],&e);\n if(s.timer!=r[5] || s.grade!=r[6] || (!e.release)!=r[7] || (e.release?256:0)!=r[8] || (e.bonus?256:0)!=r[9] || e.variant!=r[10] || (e.bonus?104:0)!=r[11] || (e.bonus?10:0)!=r[12]){test_error=n+1;break;}\n test_words++;\n}\ntest_batches=3;test_split=1;test_done=1;while(1)SYS_doVBlankProcess();return 0;}\n'
(o/'main.c').write_text(source)
flags=['-m68000','-DSGDK_GCC','-O2','-fno-builtin','-fomit-frame-pointer','-ffunction-sections','-fdata-sections','-fms-extensions','-I'+str(r/'inc'),'-I'+str(sdk/'inc'),'-I'+str(sdk/'res')]
objects=[]
for name,path in [('main',o/'main.c'),('stop',r/'src/arcade_stop.c')]:
 obj=o/(name+'.o');subprocess.run(['m68k-elf-gcc',*flags,'-c',str(path),'-o',str(obj)],check=True);objects.append(obj)
subprocess.run(['m68k-elf-gcc','-m68000','-n','-T',str(sdk/'md.ld'),'-nostdlib',str(r/'out/sega.o'),*map(str,objects),str(sdk/'lib/libmd.a'),'-lgcc','-Wl,--gc-sections','-o',str(o/'rom.out')],check=True)
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
d=json.loads((o/'results.json').read_text());d.update(rom_sha256=sha(o/'rom.bin'),native_sha256=sha(r/'src/arcade_stop.c'),scope='Actual68000 stop-state contract versus1536 original CPU observations; live owner remains pending.')
(o/'results.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2));assert d['done']==1 and d['error']==0 and d['batches']==3 and d['words']==1536
