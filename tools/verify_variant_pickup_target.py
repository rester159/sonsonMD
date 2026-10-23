#!/usr/bin/env python3
"""Actual 68000 section-bonus lifecycle against original MC6809 observations."""
from pathlib import Path
import hashlib,json,re,subprocess,sys
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/variant-pickup-target';o.mkdir(parents=True,exist_ok=True)
sdk=Path('/Users/rester159/My_apps/portForge/tools/sgdk');sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
original=sha(r/'out/rom.bin')
import runpy
v=runpy.run_path(str(r/'tools/verify_variant_pickup_lifecycle.py'))
rows=v['rows'];convert=v['convert'];fields=['x']+v['fields']
fixtures=[]
for row in rows:
 before=convert(bytes.fromhex(row['before']),10);after=convert(bytes.fromhex(row['after']),10)
 fixtures.append('{'+','.join(map(str,[row['camera'],row['variant']]+before+after))+'}')
source='#include <genesis.h>\n#include "arcade_powerup.h"\nvolatile u16 test_done,test_error,test_batches,test_words,test_split;\nstatic const u16 fixtures[][30]={'+',\n'.join(fixtures)+'};\n'
source+='int main(bool hard){u16 n;(void)hard;VDP_setEnable(FALSE);for(n=0;n<21504;n++){const u16 *r=fixtures[n];ArcadePowerup a;\n'
source+=''.join('a.%s=r[%d];\n'%(f,i+2) for i,f in enumerate(fields))
source+='if(arcade_variant_pickup_step(&a,r[0],r[1])){test_error=n+1;break;}\n'
source+='if('+ ' || '.join('a.%s!=r[%d]'%(f,i+16)for i,f in enumerate(fields))+'){test_error=n+1;break;}test_words++;}\n'
source+='test_batches=3;test_split=1;test_done=1;while(1)SYS_doVBlankProcess();return 0;}\n'
(o/'main.c').write_text(source)
flags=['-m68000','-DSGDK_GCC','-O2','-fno-builtin','-fomit-frame-pointer','-ffunction-sections','-fdata-sections','-fms-extensions','-I'+str(r/'inc'),'-I'+str(sdk/'inc'),'-I'+str(sdk/'res')]
objects=[]
for name,path in [('main',o/'main.c'),('pickup',r/'src/arcade_pickup.c'),('art',r/'src/arcade_bonus_art.c')]:
 obj=o/(name+'.o');subprocess.run(['m68k-elf-gcc',*flags,'-c',str(path),'-o',str(obj)],check=True);objects.append(obj)
subprocess.run(['m68k-elf-gcc','-m68000','-n','-T',str(sdk/'md.ld'),'-nostdlib',str(r/'out/sega.o'),*map(str,objects),str(sdk/'lib/libmd.a'),'-lgcc','-Wl,--gc-sections','-o',str(o/'rom.out')],check=True)
subprocess.run(['m68k-elf-objcopy','-O','binary',str(o/'rom.out'),str(o/'rom.bin')],check=True)
b=bytearray((o/'rom.bin').read_bytes());b.extend(bytes((-len(b))%131072));b[0x18e:0x190]=(sum(int.from_bytes(b[i:i+2],'big')for i in range(0x200,len(b),2))&65535).to_bytes(2,'big');(o/'rom.bin').write_bytes(b)
nm=subprocess.check_output(['m68k-elf-nm',str(o/'rom.out')],text=True);syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',nm,re.M)}
# Read completion and first failing fixture from the actual cartridge.
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
d=json.loads((o/'results.json').read_text());d.update(rom_sha256=sha(o/'rom.bin'),native_sha256=sha(r/'src/arcade_pickup.c'),scope='Actual68000 kind10 lifecycle contract versus21504 original CPU observations; live owner remains pending.')
(o/'results.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2));assert d['done']==1 and d['error']==0 and d['batches']==3 and d['words']==21504
