#!/usr/bin/env python3
"""Linked normal scheduler births and seven hits for both owners/all variants."""
from pathlib import Path
import re,json,hashlib,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/large-binding';o.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();rh=sha(r/'out/rom.bin')
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
s='''local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local frame=0;local scenario=0;local phase=-1;local hit=0;local terminal=false;local checks=0
local function clear(a,n)for i=0,n-1 do m:write_u8(a+i,0)end end
local function finish(err)terminal=true;local f=assert(io.open(RESULT,'w'));f:write(string.format('{"scenarios":%d,"checks":%d,"error":%s}',scenario,checks,err and string.format('%q',err)or'null'));f:close();manager.machine:exit()end
local function call(fn)
 for a=0xffb000,0xffb03e,2 do m:write_u16(a,0x4e71)end
 m:write_u16(0xffb040,0x4eb9);m:write_u32(0xffb042,fn);m:write_u16(0xffb046,0x60fe)
 cpu.state['SR'].value=0x2700;cpu.state['SP'].value=0xffbfe0;cpu.state['PC'].value=0xffb000
end
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  local owner=scenario&1;local variant=scenario//2
  if phase>=0 then assert(cpu.state['CURPC'].value&0xffffff==0xffb046,'return')end
  if phase==4 then
   for k=0,2 do local a=LARGE+k*34
    assert(m:read_u8(a+12)==1 and m:read_u8(a+18)==7,'birth active/hp')
    assert(m:read_u8(a+15)==k and m:read_u8(a+16)==variant,'birth subtype/variant')
    assert(m:read_u16(a+20)==232 and m:read_u8(a+23)==({176,160,96})[k+1],'birth position');checks=checks+3
   end
   assert(m:read_u8(WAVES+10+9*8+2)==0,'controller finished');checks=checks+1
   -- Isolate the first subtype for sequential hits; controller births above are real.
   m:write_u8(LARGE+34+12,0);m:write_u8(LARGE+68+12,0);hit=0;phase=5
  elseif phase==6 then
   assert(m:read_u8(LARGE+18)==7-hit,'HP hit '..hit);checks=checks+1
   assert(m:read_u8(LARGE+13)==(hit==7 and 2 or 0),'death state');checks=checks+1
   assert(m:read_u8(ENT+(16+owner*2)*42)==2,'shot consumed');checks=checks+1
   assert(m:read_u32(GG+owner*42+28)==(hit==7 and ({3000,4000,5000,6000})[variant+1]or 0),'reward');checks=checks+1
   if hit==7 then
    assert(m:read_u8(LARGE+17)==owner,'credit owner');checks=checks+1
    scenario=scenario+1;phase=-1
   else phase=5 end
  end
  if scenario==8 then finish();return end
  owner=scenario&1;variant=scenario//2
  if phase==-1 then
   clear(ENT,48*42);clear(GG,94);clear(FOES,240);clear(WEAPONS,72);clear(IA,96);clear(IW,10);clear(WAVES,106);clear(LARGE,102);clear(KPOOL,64)
   m:write_u8(GG+84,2);m:write_u8(SURVIVAL,0);m:write_u16(DIFFICULTY,0)
   local c=WAVES+10+9*8;m:write_u8(c+2,1);m:write_u8(c+6,variant)
   phase=0
  end
  if phase<4 then phase=phase+1;call(SCHEDULE);return end
  if phase==5 then
   clear(ENT+16*42,4*42);local shot=ENT+(16+owner*2)*42
   m:write_u8(shot,1);m:write_u8(shot+1,2);m:write_u16(shot+2,226);m:write_u16(shot+4,63);m:write_u16(shot+24,226);m:write_u8(shot+14,1);m:write_u8(shot+27,owner)
   m:write_u8(CLOCK,owner);hit=hit+1;phase=6;call(UPDATE)
  end
 end)
 if not ok then finish(err)end
end)
'''
for k,n in {'LARGE':'arcade_large_actors','KPOOL':'arcade_k_actors','ENT':'g_entities','GG':'g_game','FOES':'arcade_foes','WEAPONS':'arcade_weapons','IA':'arcade_i_actors','IW':'arcade_i_wave','WAVES':'arcade_waves','SURVIVAL':'g_survivalRun','CLOCK':'wave_clock','SCHEDULE':'entity_schedule_tick','UPDATE':'entity_update_all','DIFFICULTY':'g_arcadeCombatDifficulty'}.items():s=re.sub(r'\b'+k+r'\b',str(syms[n]),s)
s=s.replace('RESULT',json.dumps(str(o/'results.json')));(o/'run.lua').write_text(s);(o/'results.json').unlink(missing_ok=True)
with(o/'mame.log').open('w')as f:subprocess.run(['mame','genesis','-cart',str(r/'out/rom.bin'),'-window','-nomaximize','-resolution','512x448','-video','none','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_delay','0','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],cwd=r,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=90)
assert sha(r/'out/rom.bin')==rh
d=json.loads((o/'results.json').read_text());d.update(rom_sha256=rh,scope='Injected linked scheduler control, all three births, seven successive entity_update_all hits, both owners/four variants. Full encounter/render/timing parity not claimed.')
(o/'results.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2));assert d['error']is None and d['scenarios']==8
