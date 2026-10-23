#!/usr/bin/env python3
"""Two-update cross-kind slot release and subpixel inheritance regression."""
from pathlib import Path
import re,json,hashlib,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/shared-k-reuse';o.mkdir(exist_ok=True)
rom=r/'out/rom.bin';sym=r/'out/symbol.txt';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();rh=sha(rom);sh=sha(sym)
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',sym.read_text(),re.M)}
s='''local cpu=manager.machine.devices[':maincpu'];local m=cpu.spaces.program
local frame=0;local scenario=0;local cleanup=false;local pending=false;local done=false;local checks=0;local prepared=-1
local function clear(a,n)for i=0,n-1 do m:write_u8(a+i,0)end end
local function finish(err)done=true;local f=assert(io.open(RESULT,'w'));f:write(string.format('{"scenarios":%d,"checks":%d,"error":%s}',scenario,checks,err and string.format('%q',err)or'null'));f:close();manager.machine:exit()end
emu.register_frame_done(function()
 if done then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value&0xffffff==0xffb046,'return')
   if cleanup then
    cleanup=false;pending=false
   else
    local slot=scenario&3;local base=scenario<4 and WEAPONS+slot*18 or KPOOL+slot*16
    local xf=scenario<4 and 8 or 4;local yf=scenario<4 and 10 or 6
    assert(m:read_u8(base+(scenario<4 and 11 or 7))~=0,'birth')
    assert(m:read_u8(base+xf)==173,'reused X fraction '..scenario)
    assert(m:read_u8(base+yf)==219,'reused Y fraction '..scenario)
    checks=checks+3
   scenario=scenario+1;pending=false
   end
  end
  if scenario==8 then finish();return end
  if prepared~=scenario then
  clear(ENT,48*42);clear(GG,94);clear(FOES,240);clear(WEAPONS,72);clear(IA,96);clear(IW,10);clear(WAVES,106);clear(LARGE,102);clear(KPOOL,64)
  m:write_u8(SURVIVAL,0);m:write_u8(GG+84,1);m:write_u8(BUDGET,1);m:write_u8(CLOCK,1)
  -- Active player at source Y152, kept away from collision with sourceY136.
  m:write_u16(GG,60);m:write_u16(GG+2,87);m:write_u8(GG+17,0)
   prepared=scenario;cleanup=true
   for k=0,3 do
    if scenario<4 then
     local a=KPOOL+k*16;m:write_u8(a+7,1);m:write_u8(a+8,1);m:write_u16(a+2,6)
     if k==(scenario&3)then m:write_u8(a+8,3);m:write_u8(a+9,2);m:write_u8(a+4,173);m:write_u8(a+6,219)end
    else
     local a=WEAPONS+k*18;m:write_u8(a+11,1);m:write_u8(a+12,1)
     if k==(scenario&3)then m:write_u8(a+12,3);m:write_u8(a+13,2);m:write_u8(a+8,173);m:write_u8(a+10,219)end
    end
   end
  else
  if scenario<4 then
   m:write_u8(ENT,1);m:write_u8(ENT+1,7)
   m:write_u16(FOES,120);m:write_u8(FOES+3,136);m:write_u8(FOES+5,1);m:write_u8(FOES+6,3);m:write_u8(FOES+7,2);m:write_u8(FOES+9,1)
  else
   m:write_u8(LARGE+12,1);m:write_u8(LARGE+13,1);m:write_u8(LARGE+14,1);m:write_u8(LARGE+18,7);m:write_u8(LARGE+19,40)
   m:write_u16(LARGE+20,120);m:write_u8(LARGE+23,136);m:write_u8(LARGE+25,0);m:write_u8(LARGE+26,1);m:write_u8(LARGE+28,100);m:write_u8(LARGE+30,50)
  end
  end
  for a=0xffb000,0xffb03e,2 do m:write_u16(a,0x4e71)end
  m:write_u16(0xffb040,0x4eb9);m:write_u32(0xffb042,UPDATE);m:write_u16(0xffb046,0x60fe)
  cpu.state['SR'].value=0x2700;cpu.state['SP'].value=0xffbfe0;cpu.state['PC'].value=0xffb000;pending=true
 end)
 if not ok then finish(err)end
end)
'''
for k,n in {'LARGE':'arcade_large_actors','KPOOL':'arcade_k_actors','ENT':'g_entities','GG':'g_game','FOES':'arcade_foes','WEAPONS':'arcade_weapons','IA':'arcade_i_actors','IW':'arcade_i_wave','WAVES':'arcade_waves','SURVIVAL':'g_survivalRun','CLOCK':'wave_clock','BUDGET':'arcade_shot_budget','UPDATE':'entity_update_all'}.items():s=re.sub(r'\b'+k+r'\b',str(syms[n]),s)
s=s.replace('RESULT',json.dumps(str(o/'results.json')));(o/'run.lua').write_text(s);(o/'results.json').unlink(missing_ok=True)
with(o/'mame.log').open('w')as f:subprocess.run(['mame','genesis','-cart',str(rom),'-window','-nomaximize','-resolution','512x448','-video','none','-noreadconfig','-sound','none','-nothrottle','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],cwd=r,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=90)
assert sha(rom)==rh and sha(sym)==sh
d=json.loads((o/'results.json').read_text());d.update(rom_sha256=rh,symbol_sha256=sh,scope='Two injected live updates per slot: cleanup then opposite-kind birth, both directions, all four slots. Checks retained X/Y fractions; other shared fields and natural reachability excluded.')
(o/'results.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2));assert d['error']is None and d['scenarios']==8
