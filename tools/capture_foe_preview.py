#!/usr/bin/env python3
"""Capture five windowless cartridge scenes per player mode, with real controls.
Lives are refilled so the capture sequence continues; no enemy states are seeded.
"""
from pathlib import Path
import json,re,shutil,subprocess,hashlib
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/foe-preview';o.mkdir(parents=True,exist_ok=True)
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
identity=hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest();shots=[]
for players in [1,2]:
 d=o/f'{players}p';d.mkdir(exist_ok=True)
 s='''local m=manager.machine.devices[":maincpu"].spaces.program
local fr=0;local captures=0
local function pad(k,n,v)manager.machine.ioport.ports[":ctrl"..k..":mdpad:PAD"].fields["P"..k.." "..n]:set_value(v)end
emu.register_frame_done(function()
 fr=fr+1
 for k=1,2 do for _,n in ipairs({"Start","C","B","Up","Down","Right"})do pad(k,n,0)end end
 if fr<700 and fr%24<12 then pad(1,"Start",1)end
 if fr>=940 and fr<970 then pad(1,"C",1)end
 if fr>=1000 and fr<1012 then pad(1,"Start",1)end
 if PLAYERS==2 then
  if fr>=1040 and fr<1060 then pad(2,"C",1)end
  if fr>=1080 and fr<1092 then pad(2,"Start",1)end
 end
 if fr>=1200 then
  for k=1,PLAYERS do
   m:write_u8(GG+(k-1)*42+32,99);pad(k,"Right",1)
   if fr%16<3 then pad(k,"B",1)end
   local phase=(fr+k*60)%240
   if phase<32 then pad(k,"Up",1)elseif phase>=90 and phase<122 then pad(k,"Down",1)end
  end
 end
 if fr==1400 or fr==1800 or fr==2200 or fr==3000 or fr==4000 then
  assert(m:read_u16(LW)>0 and m:read_u8(GG+84)==PLAYERS,"Capture did not reach requested game")
  manager.machine.video:snapshot();captures=captures+1
 end
 if fr==4010 then assert(captures==5);manager.machine:exit()end
end)
'''
 for key,v in {'GG':syms['g_game'],'LW':syms['g_levelW'],'PLAYERS':players}.items():s=re.sub(r'\b'+key+r'\b',str(v),s)
 (d/'run.lua').write_text(s)
 raw=d/'raw'/identity[:12];raw.mkdir(parents=True,exist_ok=True)
 for old in (raw/'genesis').glob('*.png'):old.unlink()
 subprocess.run(['mame','genesis','-cart',str(r/'out/rom.bin'),'-window','-nomaximize','-resolution','512x448','-video','none','-keyboardprovider','none','-mouseprovider','none','-joystickprovider','none','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_script',str(d/'run.lua'),'-cfg_directory',str(d/'cfg'),'-nvram_directory',str(d/'nvram'),'-snapshot_directory',str(raw)],cwd=r,stdout=(d/'mame.log').open('w'),stderr=subprocess.STDOUT,check=True,timeout=180)
 images=sorted((raw/'genesis').glob('*.png'));assert len(images)==5,len(images)
 for k,p in enumerate(images,1):
  target=o/f'foes-{players}p-{k:02d}.png';shutil.copy2(p,target);shots.append(str(target))
assert hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest()==identity
(o/'manifest.json').write_text(json.dumps({'rom_sha256':identity,'screenshots':shots,'scope':'Controller-driven one/two-player gameplay; lives refilled, enemy states untouched'},indent=2)+'\n');print('Captured',len(shots),'gameplay screenshots')
