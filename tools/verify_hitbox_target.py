#!/usr/bin/env python3
"""Check linked 68000 hitbox functions against original CPU observations."""
from pathlib import Path
import hashlib,json,re,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/hitbox-target';o.mkdir(parents=True,exist_ok=True)
trace=r/'out/parity-audit/hitbox-oracle.csv';report=json.loads((r/'out/parity-audit/hitbox-comparison.json').read_text());identity=hashlib.sha256(trace.read_bytes()).hexdigest();assert identity==report['trace_sha256']
rom=r/'out/rom.bin';rh=hashlib.sha256(rom.read_bytes()).hexdigest()
symbol=r/'out/symbol.txt';sh=hashlib.sha256(symbol.read_bytes()).hexdigest()
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',symbol.read_text(),re.M)}
s='''local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local input=assert(io.open(TRACE,"r"));input:read("*l")
local frame=0;local pending=nil;local terminal=false;local cases=0
local function finish(err)
 terminal=true;input:close();local f=assert(io.open(RESULT,"w"));f:write(string.format('{"cases":%d,"field_checks":%d,"error":%s}',cases,cases,err and string.format('%q',err)or"null"));f:close();manager.machine:exit()
end
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state["CURPC"].value&0xffffff==0xffb046,"routine did not return")
   assert(cpu.state["D0"].value&255==pending[6],"hit result case "..cases)
   cases=cases+1
  end
  local line=input:read("*l");if not line then finish();return end
  local d={}for n in line:gmatch("%d+")do d[#d+1]=tonumber(n)end;pending=d
  m:write_u8(0xffb200,d[2])
  for x=0xffb000,0xffb03e,2 do m:write_u16(x,0x4e71)end
  m:write_u16(0xffb040,0x4eb9);m:write_u32(0xffb042,OVERLAP);m:write_u16(0xffb046,0x60fe)
  m:write_u32(0xffbfe0,d[2]);m:write_u32(0xffbfe4,d[3])
  m:write_u32(0xffbfe8,d[4]);m:write_u32(0xffbfec,d[5])
  cpu.state["SR"].value=0x2700;cpu.state["SP"].value=0xffbfe0;cpu.state["PC"].value=0xffb000
 end)
 if not ok then finish(err)end
end)
'''
for k,v in {'TRACE':json.dumps(str(trace)),'RESULT':json.dumps(str(o/'results.json')),'OVERLAP':syms['arcade_gh_shot_overlap']}.items():s=re.sub(r'\b'+k+r'\b',str(v),s)
(o/'run.lua').write_text(s)
subprocess.run(['mame','genesis','-cart',str(rom),'-window','-nomaximize','-resolution','512x448','-video','none','-keyboardprovider','none','-mouseprovider','none','-joystickprovider','none','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],cwd=r,stdout=(o/'mame.log').open('w'),stderr=subprocess.STDOUT,check=True,timeout=300)
assert hashlib.sha256(rom.read_bytes()).hexdigest()==rh
assert hashlib.sha256(symbol.read_bytes()).hexdigest()==sh
assert hashlib.sha256(trace.read_bytes()).hexdigest()==identity
result=json.loads((o/'results.json').read_text());result.update(rom_sha256=rh,symbol_sha256=sh,trace_sha256=identity)
(o/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));assert result['error']is None and result['cases']==7748,result
