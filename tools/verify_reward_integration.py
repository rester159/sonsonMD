#!/usr/bin/env python3
"""Compiled entity-update integration: both G/H families, both players, ordinary/final kills."""
from pathlib import Path
import argparse,hashlib,json,re,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--baseline',type=Path);p.add_argument('--expect-failure',action='store_true');a=p.parse_args()
r=Path(__file__).resolve().parents[1];source=a.baseline or r/'out';o=r/'out/parity-audit'/('collision-pages-baseline'if a.baseline else'reward-integration');o.mkdir(parents=True,exist_ok=True)
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(source/'symbol.txt').read_text(),re.M)}
s='''local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local frame=0;local test=-1;local terminal=false;local checks=0
local function clear(a,n)for i=0,n-1 do m:write_u8(a+i,0)end end
local function finish(err)
 terminal=true;local f=assert(io.open(RESULT,"w"));f:write(string.format('{"cases":%d,"checks":%d,"error":%s}',test+1,checks,err and string.format('%q',err)or"null"));f:close();manager.machine:exit()
end
local function run()
 local family=(test%8)//4;local owner=(test//2)%2;local last=test%2
 clear(ENT,48*42);clear(GG,94);clear(FOES,240);clear(WEAPONS,72)
 m:write_u8(GG+84,2);m:write_u8(SURVIVAL,0);m:write_u8(CLOCK,owner);m:write_u16(DIFFICULTY,0x1cff)
 m:write_u8(RESERVATION+family,last==1 and 1 or 2)
 for k=0,1 do m:write_u16(GG+k*42,32);m:write_u16(GG+k*42+2,32);m:write_u8(GG+k*42+32,3)end
 local e=ENT+family*6*42;local a=FOES+family*6*20
 m:write_u8(e,1);m:write_u8(e+1,family==0 and 7 or 1);m:write_u8(e+17,1)
 m:write_u16(a,100);m:write_u8(a+3,136);m:write_u8(a+5,1);m:write_u8(a+6,2);m:write_u8(a+7,1);m:write_u8(a+8,80);m:write_u8(a+9,3);m:write_u8(a+14,family==0 and 1 or 10)
 -- H cases deliberately overlap G: source H scan must consume the shot first.
 if family==1 then
  m:write_u8(ENT,1);m:write_u8(ENT+1,7);m:write_u8(ENT+17,1)
  m:write_u16(FOES,108);m:write_u8(FOES+3,136);m:write_u8(FOES+5,1);m:write_u8(FOES+6,2);m:write_u8(FOES+7,1);m:write_u8(FOES+8,80);m:write_u8(FOES+9,3);m:write_u8(FOES+14,1)
 end
 -- Ordinary AD10 owns slots16/17 for P1 and18/19 for P2.
 local shot=ENT+((family==1 and 17 or 16)+owner*2)*42
 m:write_u8(shot,1);m:write_u8(shot+1,2);m:write_u8(shot+14,1);m:write_u16(shot+2,family==1 and 98 or 94);m:write_u16(shot+4,103);m:write_u16(shot+24,100);m:write_u8(shot+27,owner)
 if family==1 then
  -- Global entity order is reversed relative to the source shot slots.
  local other=ENT+(16+owner*2)*42;m:write_u8(other,1);m:write_u8(other+1,2);m:write_u8(other+14,1);m:write_u16(other+2,90);m:write_u16(other+4,103);m:write_u16(other+24,96);m:write_u8(other+27,owner);m:write_u8(other+26,1)
 end
 if test>=8 then
  -- Range-expired puff is still source active=1 and remains a shot target.
  m:write_u16(shot+2,family==1 and 104 or 100);m:write_u8(shot+15,1);m:write_u8(shot+21,3);m:write_u8(shot+16,3);m:write_u8(shot+37,2);m:write_u8(shot+38,1);m:write_u8(shot+39,3)
 end
 for x=0xffb000,0xffb03e,2 do m:write_u16(x,0x4e71)end
 m:write_u16(0xffb040,0x4eb9);m:write_u32(0xffb042,FN);m:write_u16(0xffb046,0x60fe)
 cpu.state["SR"].value=0x2700;cpu.state["SP"].value=0xffbff0;cpu.state["PC"].value=0xffb000
end
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if test>=0 then
   assert(cpu.state["CURPC"].value&0xffffff==0xffb046,"routine did not return")
   local family=(test%8)//4;local owner=(test//2)%2;local last=test%2;local a=FOES+family*120;local e=ENT+family*252
   local got={m:read_u32(GG+owner*42+28),m:read_u32(GG+(1-owner)*42+28),m:read_u8(a+5),m:read_u8(a+6),m:read_u8(a+7),m:read_u8(a+18),m:read_u8(e),m:read_u8(ENT+((family==1 and 17 or 16)+owner*2)*42),m:read_u8(RESERVATION+family),m:read_u16(DIFFICULTY)}
   local expected={150+(last==1 and(family==0 and 2000 or 1000)or 0),0,2,6,last==1 and 4 or 0,owner,2,2,last==1 and 0 or 1,0x1cff+(family==0 and 0x33 or 0x19)+(last==1 and(family==0 and 0x4c or 0x40)or 0)}
   if family==1 then checks=checks+2;assert(m:read_u8(FOES+5)==1,"H must be hit before overlapping G");assert(m:read_u8(ENT+(16+owner*2)*42)==1,"shot slot 1 must survive despite its lower entity index")end
   checks=checks+1;assert(m:read_u8(ENT+((family==1 and 17 or 16)+owner*2)*42+21)==2,"impact must restart the source puff state")
   for k=1,#got do checks=checks+1;assert(got[k]==expected[k],"case "..test.." field "..k.." actual "..got[k].." expected "..expected[k])end
  end
  if test==15 then finish();return end;test=test+1;run()
 end)
 if not ok then finish(err)end
end)
'''
for k,v in {'RESULT':json.dumps(str(o/'results.json')),'ENT':syms['g_entities'],'GG':syms['g_game'],'SURVIVAL':syms['g_survivalRun'],'CLOCK':syms['wave_clock'],'FN':syms['entity_update_all'],'FOES':syms['arcade_foes'],'WEAPONS':syms['arcade_weapons'],'DIFFICULTY':syms['g_arcadeCombatDifficulty'],'RESERVATION':syms['formation_reservation']}.items():s=re.sub(r'\b'+k+r'\b',str(v),s)
(o/'run.lua').write_text(s);rom=source/'rom.bin';identity=hashlib.sha256(rom.read_bytes()).hexdigest()
subprocess.run(['mame','genesis','-cart',str(rom.resolve()),'-window','-nomaximize','-resolution','512x448','-video','none','-keyboardprovider','none','-mouseprovider','none','-joystickprovider','none','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],cwd=r,stdout=(o/'mame.log').open('w'),stderr=subprocess.STDOUT,check=True,timeout=120)
result=json.loads((o/'results.json').read_text());result['rom_sha256']=identity;(o/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
if a.expect_failure:assert result['error']is not None and result['error'].get('phase')==1,result
else:assert result['error']is None and result['cases']==16,result
