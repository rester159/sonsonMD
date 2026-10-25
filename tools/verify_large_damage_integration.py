#!/usr/bin/env python3
"""Linked large spawn and seven sequential collisions, both shot owners."""
import argparse,hashlib,json,re,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--baseline',type=Path);args=p.parse_args()
r=Path(__file__).resolve().parents[1];build=args.baseline or r/'out';o=r/'out/parity-audit'/('large-hp-baseline'if args.baseline else'large-hp-target');o.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
report=json.loads((r/'out/parity-audit/large-damage-comparison.json').read_text());assert report['initial_hp']==7
assert sha(r/'out/parity-audit/large-damage-oracle.csv')==report['trace_sha256']
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(build/'symbol.txt').read_text(),re.M)}
s='''local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local frame=0;local owner=0;local phase=-1;local terminal=false;local checks=0
local function clear(a,n)for i=0,n-1 do m:write_u8(a+i,0)end end
local function finish(err)
 terminal=true;local f=assert(io.open(RESULT,'w'));f:write(string.format('{"checks":%d,"error":%s}',checks,err and string.format('%q',err)or'null'));f:close();manager.machine:exit()
end
local function call(fn,args)
 for a=0xffb000,0xffb03e,2 do m:write_u16(a,0x4e71)end
 m:write_u16(0xffb040,0x4eb9);m:write_u32(0xffb042,fn);m:write_u16(0xffb046,0x60fe)
 for i,v in ipairs(args or {})do m:write_u32(0xffbfe0+(i-1)*4,v)end
 cpu.state['SR'].value=0x2700;cpu.state['SP'].value=0xffbfe0;cpu.state['PC'].value=0xffb000
end
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  local e=ENT+24*42
  if phase>=0 then
   assert(cpu.state['CURPC'].value&0xffffff==0xffb046,'return')
   assert(m:read_u8(e+17)==7-phase,'HP owner '..owner..' hit '..phase..' got '..m:read_u8(e+17));checks=checks+1
   assert(m:read_u8(e+15)==(phase==7 and 1 or 0),'death timing');checks=checks+1
   if phase>0 then assert(m:read_u8(ENT+(16+owner*2)*42)==2,'shot consumed');checks=checks+1 end
   if phase==7 then owner=owner+1;phase=-1 end
  end
  if owner==2 then finish();return end
  if phase==-1 then
   clear(ENT,48*42);clear(GG,94);clear(FOES,240);clear(WEAPONS,72);clear(IA,96);clear(IW,10);clear(WAVES,106)
   m:write_u8(GG+84,2);m:write_u8(GG+34,3);m:write_u8(GG+76,3);m:write_u8(SURVIVAL,0)
   phase=0;call(SPAWN,{11,100,104,24,25});return
  end
  clear(ENT+16*42,4*42)
  local shot=ENT+(16+owner*2)*42
  m:write_u8(shot,1);m:write_u8(shot+1,2);m:write_u16(shot+2,m:read_u16(e+2)-6);m:write_u16(shot+4,104)
  m:write_u16(shot+24,m:read_u16(e+2));m:write_u8(shot+14,1);m:write_u8(shot+27,owner)
  m:write_u8(CLOCK,owner);phase=phase+1;call(UPDATE)
 end)
 if not ok then finish(err)end
end)
'''
for k,n in {'ENT':'g_entities','GG':'g_game','FOES':'arcade_foes','WEAPONS':'arcade_weapons','IA':'arcade_i_actors','IW':'arcade_i_wave','WAVES':'arcade_waves','SURVIVAL':'g_survivalRun','CLOCK':'wave_clock','SPAWN':'entity_spawn_range','UPDATE':'entity_update_all'}.items():s=re.sub(r'\b'+k+r'\b',str(syms[n]),s)
s=s.replace('RESULT',json.dumps(str(o/'results.json')));(o/'run.lua').write_text(s);(o/'results.json').unlink(missing_ok=True)
rh=sha(build/'rom.bin');sh=sha(build/'symbol.txt')
with (o/'mame.log').open('w')as f:subprocess.run(['mame','genesis','-cart',str((build/'rom.bin').resolve()),'-window','-nomaximize','-resolution','512x448','-video','none','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_delay','0','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],cwd=r,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=90)
assert rh==sha(build/'rom.bin')and sh==sha(build/'symbol.txt')
d=json.loads((o/'results.json').read_text());d.update(rom_sha256=rh,source_trace_sha256=report['trace_sha256'],scope='Linked initializer and seven sequential entity_update_all hits for both owners, injected encounter. Only HP/death timing/shot consumption; movement, hitbox and rewards not claimed.')
(o/'results.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2));assert d['error']is None and d['checks']==46
