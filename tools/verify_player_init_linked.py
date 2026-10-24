#!/usr/bin/env python3
"""Check normal-player initialization in the linked Genesis cartridge."""
from pathlib import Path
import argparse,hashlib,json,re,subprocess
p=argparse.ArgumentParser();p.add_argument("--baseline",type=Path);p.add_argument("--expect-failure",action="store_true");args=p.parse_args()
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit'/('player-init-baseline'if args.baseline else'player-init-linked');o.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
trace=r/'out/parity-audit/player-init-target.csv';report=json.loads((r/'out/parity-audit/player-init-comparison.json').read_text());identity=sha(trace);assert identity==report['target_sha256']
source=r/'src/arcade_player.c';assert sha(source)==report['native_sha256']
build=args.baseline or r/'out';rom=build/'rom.bin';rh=sha(rom)
symbol=build/'symbol.txt';symbol_hash=sha(symbol)
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',symbol.read_text(),re.M)}
address=syms['arcade_player_step_events']
s='''local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local input=assert(io.open(TRACE,"r"));input:read("*l")
local frame=0;local pending=nil;local terminal=false;local cases=0
local function finish(err)
 terminal=true;input:close();local f=assert(io.open(RESULT,"w"));f:write(string.format('{"cases":%d,"field_checks":%d,"error":%s}',cases,cases*17,err and string.format('%q',err)or"null"));f:close();manager.machine:exit()
end
local function bytes(a,s)
 for k=0,#s//2-1 do m:write_u8(a+k,tonumber(s:sub(k*2+1,k*2+2),16))end
end
local function compare(a,s,skip)
 for k=0,#s//2-1 do if k~=skip then assert(m:read_u8(a+k)==tonumber(s:sub(k*2+1,k*2+2),16),string.format('case %d address %x',cases,a+k))end end
end
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state["CURPC"].value&0xffffff==0xffb046,"routine did not return")
   compare(0xffb200,pending[5]);compare(0xffb280,pending[6])
   cases=cases+1
  end
  local line=input:read("*l");if not line then finish();return end
  local d={}for n in line:gmatch("[^,]+")do d[#d+1]=n end
  for _,k in ipairs({2,3,4})do d[k]=tonumber(d[k])end;pending=d
  bytes(0xffb200,d[1]);m:write_u32(0xffb280,0);for k=0,13 do m:write_u8(0xffb240+k,0)end;m:write_u8(0xffb245,5);m:write_u8(0xffb24b,d[2])
  for x=0xffb000,0xffb03e,2 do m:write_u16(x,0x4e71)end
  m:write_u16(0xffb040,0x4eb9);m:write_u32(0xffb042,ENTRY);m:write_u16(0xffb046,0x60fe)
  m:write_u32(0xffbfe0,0xffb200);m:write_u32(0xffbfe4,0xffb240);m:write_u32(0xffbfe8,d[3]);m:write_u32(0xffbfec,d[4]);m:write_u32(0xffbff0,0);m:write_u32(0xffbff4,0);m:write_u32(0xffbff8,0xffb280)
  cpu.state["SR"].value=0x2700;cpu.state["SP"].value=0xffbfe0;cpu.state["PC"].value=0xffb000
 end)
 if not ok then finish(err)end
end)
'''
for k,v in {'TRACE':str(trace),'RESULT':str(o/'results.json')}.items():s=re.sub(r'\b'+k+r'\b',json.dumps(v),s)
s=s.replace('ENTRY',str(address))
(o/'run.lua').write_text(s)
subprocess.run(['mame','genesis','-cart',str(rom),'-window','-nomaximize','-resolution','512x448','-video','none','-keyboardprovider','none','-mouseprovider','none','-joystickprovider','none','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],cwd=r,stdout=(o/'mame.log').open('w'),stderr=subprocess.STDOUT,check=True,timeout=300)
assert sha(rom)==rh and sha(trace)==identity and sha(source)==report['native_sha256']
assert sha(symbol)==symbol_hash
result=json.loads((o/'results.json').read_text());result.update(scope='Linked cartridge locomotion initialization',rom_sha256=rh,symbol_sha256=symbol_hash,source_sha256=json.loads((build/'build-manifest.json').read_text())['source_sha256']['src/arcade_player.c'],trace_sha256=identity)
(o/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
if args.expect_failure:assert result['error']is not None,result
else:assert result['error']is None and result['cases']==report['cases'],result
