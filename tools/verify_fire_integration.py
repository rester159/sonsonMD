#!/usr/bin/env python3
"""Target: dedicated shot slots survive a full ordinary pool, counters and reuse."""
from pathlib import Path
import hashlib,json,re,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/fire-integration';o.mkdir(parents=True,exist_ok=True)
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
identity=hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest()
s='''local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local frame=0;local test=-1;local terminal=false;local checks=0
local function check(ok,why)checks=checks+1;assert(ok,why.." case "..test)end
local function clear(a,n)for k=0,n-1 do m:write_u8(a+k,0)end end
local function finish(err)
 terminal=true;local f=assert(io.open(RESULT,"w"));f:write(string.format('{"cases":%d,"checks":%d,"error":%s}',test+1,checks,err and string.format('%q',err)or"null"));f:close();manager.machine:exit()
end
local function run()
 local owner=test//6;local phase=test%6;local first=ENT+(16+owner*2)*42
 if phase==0 then
  clear(ENT,48*42);clear(GG,94);clear(FOES,240);clear(WEAPONS,72);m:write_u8(SURVIVAL,0);m:write_u8(CLOCK,owner);m:write_u8(AVAILABLE,2);m:write_u8(AVAILABLE+1,2)
  for k=24,47 do m:write_u8(ENT+k*42,1)end
  for k=0,1 do m:write_u16(first+k*42+6,0x12+k);m:write_u16(first+k*42+8,0x34+k)end
 end
 if phase==4 then for k=0,1 do local a=first+k*42;m:write_u8(a+15,1);m:write_u8(a+21,3);m:write_u8(a+16,1)end end
 local update=phase==2 or phase==4
 for x=0xffb000,0xffb03e,2 do m:write_u16(x,0x4e71)end
 m:write_u16(0xffb040,0x4eb9);m:write_u32(0xffb042,update and UPDATE or FIRE);m:write_u16(0xffb046,0x60fe)
 m:write_u32(0xffbfe0,owner);m:write_u32(0xffbfe4,phase==1 and 120 or 100);m:write_u32(0xffbfe8,103);m:write_u32(0xffbfec,owner==0 and 1 or 3)
 cpu.state["SR"].value=0x2700;cpu.state["SP"].value=0xffbfe0;cpu.state["PC"].value=0xffb000
end
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if test>=0 then
   check(cpu.state["CURPC"].value&0xffffff==0xffb046,"routine return")
   local owner=test//6;local phase=test%6;local first=ENT+(16+owner*2)*42
   check(m:read_u8(AVAILABLE+owner)==((phase==2 or phase==3)and 0 or 2),"availability")
   if phase~=2 and phase~=4 then check(cpu.state["D0"].value&65535==(phase==3 and 0 or 1),"fire result")end
   for k=0,1 do
    local a=first+k*42;local active=(phase==4 or ((phase==0 or phase==5)and k==1))and 0 or 1
    check(m:read_u8(a)==active,"stable slot active")
    check(m:read_u16(a+6)==0x12+k and m:read_u16(a+8)==0x34+k,"fraction retained")
    if active==1 then check(m:read_u8(a+27)==owner and m:read_u8(a+26)==k,"owner/slot identity")end
   end
   for k=24,47 do check(m:read_u8(ENT+k*42)==1 and m:read_u8(ENT+k*42+1)==0,"ordinary pool untouched")end
  end
  if test==11 then finish();return end;test=test+1;run()
 end)
 if not ok then finish(err)end
end)
'''
for k,v in {'RESULT':json.dumps(str(o/'results.json')),'ENT':syms['g_entities'],'GG':syms['g_game'],'FOES':syms['arcade_foes'],'WEAPONS':syms['arcade_weapons'],'SURVIVAL':syms['g_survivalRun'],'CLOCK':syms['wave_clock'],'AVAILABLE':syms['player_shot_available'],'UPDATE':syms['entity_update_all'],'FIRE':syms['entity_player_fire']}.items():s=re.sub(r'\b'+k+r'\b',str(v),s)
(o/'run.lua').write_text(s)
subprocess.run(['mame','genesis','-cart',str(r/'out/rom.bin'),'-window','-nomaximize','-resolution','512x448','-video','none','-keyboardprovider','none','-mouseprovider','none','-joystickprovider','none','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],cwd=r,stdout=(o/'mame.log').open('w'),stderr=subprocess.STDOUT,check=True,timeout=120)
assert hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest()==identity
result=json.loads((o/'results.json').read_text());result['rom_sha256']=identity;(o/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));assert result['error']is None and result['cases']==12,result
