#!/usr/bin/env python3
"""Compiled collision-page regression: both owners overlap one killable body.
An old every-player-every-tick ROM incorrectly always credits slot one's shot.
"""
from pathlib import Path
import argparse,hashlib,json,re,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--baseline',type=Path);p.add_argument('--expect-failure',action='store_true');a=p.parse_args()
r=Path(__file__).resolve().parents[1];source=a.baseline or r/'out';o=r/'out/parity-audit'/('collision-pages-baseline'if a.baseline else'collision-pages-target');o.mkdir(parents=True,exist_ok=True)
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(source/'symbol.txt').read_text(),re.M)}
s='''local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local frame=0;local phase=-1;local terminal=false;local checks=0
local function finish(error)
 if terminal then return end;terminal=true
 local f=assert(io.open(RESULT,"w"));f:write(string.format('{"cases":%d,"checks":%d,"error":%s}',phase+1,checks,error or "null"));f:close();manager.machine:exit()
end
local function clear(a,n)for k=0,n-1 do m:write_u8(a+k,0)end end
local function run()
 clear(ENT,48*42);clear(GG,94)
 m:write_u8(GG+84,2);m:write_u8(SURVIVAL,0);m:write_u8(CLOCK,phase)
 for k=0,1 do local p=GG+k*42;m:write_u16(p,32);m:write_u16(p+2,32);m:write_u8(p+16,k);m:write_u8(p+32,3)end
 local enemy=ENT+24*42;m:write_u8(enemy,1);m:write_u8(enemy+1,12);m:write_u16(enemy+2,100);m:write_u16(enemy+4,100);m:write_u8(enemy+16,254);m:write_u8(enemy+17,1);m:write_u8(enemy+36,32);m:write_u8(enemy+38,4)
 for k=0,1 do local shot=ENT+(17+k)*42
  m:write_u8(shot,1);m:write_u8(shot+1,2);m:write_u8(shot+14,1);m:write_u16(shot+2,100);m:write_u16(shot+4,100);m:write_u16(shot+24,100);m:write_u8(shot+27,k);m:write_u8(shot+36,15);m:write_u8(shot+38,4)
 end
 for x=0xffb000,0xffb03e,2 do m:write_u16(x,0x4e71)end
 m:write_u16(0xffb040,0x4eb9);m:write_u32(0xffb042,FN);m:write_u16(0xffb046,0x60fe)
 cpu.state["SR"].value=0x2700;cpu.state["SP"].value=0xffbff0;cpu.state["PC"].value=0xffb000
end
emu.register_frame_done(function()
 frame=frame+1;if terminal then manager.machine:exit();return end;if frame<60 then return end
 if phase>=0 then
  if cpu.state["CURPC"].value&0xffffff~=0xffb046 then finish('{"failure":"routine did not return"}');return end
  for k=0,1 do
   local score=m:read_u32(GG+k*42+28);local puff=m:read_u8(ENT+(17+k)*42+15);checks=checks+2
   if score~=(k==phase and 100 or 0)or puff~=(k==phase and 1 or 0)then
    finish(string.format('{"phase":%d,"owner":%d,"score":%d,"shot_state":%d}',phase,k,score,puff));return
   end
  end
 end
 if phase==1 then finish();return end;phase=phase+1;run()
end)
'''
if 'legacy_pool_used' in syms:
 s=s.replace(' local enemy=ENT+24*42;',f" m:write_u8({syms['legacy_pool_used']},1) -- Direct RAM allocation fixture.\n local enemy=ENT+24*42;")
for k,v in {'RESULT':json.dumps(str(o/'results.json')),'ENT':syms['g_entities'],'GG':syms['g_game'],'SURVIVAL':syms['g_survivalRun'],'CLOCK':syms['wave_clock'],'FN':syms['entity_update_all']}.items():s=re.sub(r'\b'+k+r'\b',str(v),s)
(o/'run.lua').write_text(s);rom=source/'rom.bin';identity=hashlib.sha256(rom.read_bytes()).hexdigest()
subprocess.run(['mame','genesis','-cart',str(rom.resolve()),'-window','-nomaximize','-resolution','512x448','-video','none','-keyboardprovider','none','-mouseprovider','none','-joystickprovider','none','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],cwd=r,stdout=(o/'mame.log').open('w'),stderr=subprocess.STDOUT,check=True,timeout=120)
result=json.loads((o/'results.json').read_text());result['rom_sha256']=identity;(o/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
if a.expect_failure:assert result['error']is not None and result['error'].get('phase')==1,result
else:assert result['error']is None and result['cases']==2,result
