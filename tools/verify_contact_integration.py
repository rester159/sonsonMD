#!/usr/bin/env python3
"""Compiled entity loop: ordinary contact boundaries, flags and player ownership."""
from pathlib import Path
import hashlib,json,re,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/contact-integration';o.mkdir(parents=True,exist_ok=True)
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
rh=hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest()
s='''local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local frame=0;local test=-1;local checks=0;local terminal=false
local offsets={0,7,8,-7,-8};local flags={1,2,128}
local function clear(a,n)for k=0,n-1 do m:write_u8(a+k,0)end end
local function check(v,why)checks=checks+1;assert(v,why..' test '..test)end
local function values()
 return offsets[test%5+1],offsets[(test//5)%5+1],(test//25)%2,flags[(test//50)%3+1],(test//150)%2
end
local function run()
 clear(ENT,48*42);clear(GG,94);clear(FOES,240);clear(WEAPONS,72);clear(IA,96);clear(IW,10);clear(WAVES,106)
 local dx,dy,owner,active,gate=values()
 m:write_u8(ILIVE,1);m:write_u8(SURVIVAL,0);m:write_u8(GG+84,2);m:write_u8(CLOCK,owner)
 m:write_u8(GG+(1-owner)*42+34,3)
 local p=GG+owner*42;m:write_u16(p,100+dx);m:write_u16(p+2,103-dy);m:write_u8(p+39,gate)
 m:write_u16(IA+8,100);m:write_u8(IA+11,136);m:write_u8(IA+13,active);m:write_u8(IA+14,1);m:write_u8(IA+17,3);m:write_u8(IA+21,1)
 local e=ENT+20*42;m:write_u8(e,active);m:write_u8(e+1,10);m:write_u8(e+27,128)
 for x=0xffb000,0xffb03e,2 do m:write_u16(x,0x4e71)end
 m:write_u16(0xffb040,0x4eb9);m:write_u32(0xffb042,FN);m:write_u16(0xffb046,0x60fe)
 cpu.state['SR'].value=0x2700;cpu.state['SP'].value=0xffbfe0;cpu.state['PC'].value=0xffb000
end
local function finish(err)
 terminal=true;local f=assert(io.open(RESULT,'w'));f:write(string.format('{"updates":%d,"checks":%d,"error":%s}',test+1,checks,err and string.format('%q',err)or'null'));f:close();manager.machine:exit()
end
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if test>=0 then
   check(cpu.state['CURPC'].value&0xffffff==0xffb046,'return')
   local dx,dy,owner,active,gate=values();local hit=math.abs(dx)<=7 and math.abs(dy)<=7 and active~=2 and gate==0
   check(m:read_u8(GG+owner*42+38)==(hit and 1 or 0),'contact boundary/flags')
   check(m:read_u8(GG+(1-owner)*42+38)==0,'other owner')
  end
  if test==299 then finish();return end;test=test+1;run()
 end)
 if not ok then finish(err)end
end)
'''
for k,v in {'ENT':syms['g_entities'],'GG':syms['g_game'],'FOES':syms['arcade_foes'],'WEAPONS':syms['arcade_weapons'],'IA':syms['arcade_i_actors'],'IW':syms['arcade_i_wave'],'WAVES':syms['arcade_waves'],'ILIVE':syms['arcade_i_live'],'IRES':syms['arcade_i_reservation'],'CLOCK':syms['wave_clock'],'SURVIVAL':syms['g_survivalRun'],'DIFFICULTY':syms['g_arcadeCombatDifficulty'],'FN':syms['entity_update_all'],'RESULT':json.dumps(str(o/'results.json'))}.items():s=re.sub(r'\b'+k+r'\b',str(v),s)
(o/'run.lua').write_text(s)
subprocess.run(['mame','genesis','-cart',str(r/'out/rom.bin'),'-window','-nomaximize','-resolution','512x448','-video','none','-keyboardprovider','none','-mouseprovider','none','-joystickprovider','none','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],cwd=r,stdout=(o/'mame.log').open('w'),stderr=subprocess.STDOUT,timeout=120,check=True)
assert hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest()==rh
result=json.loads((o/'results.json').read_text());result['rom_sha256']=rh;(o/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));assert result['error']is None and result['updates']==300,result
