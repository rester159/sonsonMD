#!/usr/bin/env python3
"""Check linked shot pixels and target SAT flip/visibility for both directions."""
from pathlib import Path
import hashlib,json,re,struct,subprocess
import gen_assets as G
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/shot-render';o.mkdir(parents=True,exist_ok=True)
rom=(r/'out/rom.bin').read_bytes();identity=hashlib.sha256(rom).hexdigest()
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
g=G.Roms(str(r/'arcade'));tiles=[]
for anim,code in [(15,70),(37,78)]:
 address=int.from_bytes(rom[syms['ss_anims']+anim*6:syms['ss_anims']+anim*6+4],'big')
 for frame in range(2):
  tile=int.from_bytes(rom[address+frame*4:address+frame*4+2],'big');tiles.append(tile)
  raw=rom[syms['ss_anim_tiles']+(tile-290)*32:syms['ss_anim_tiles']+(tile-290)*32+128]
  assert raw==b''.join(struct.pack('>I',v)for v in G.bake_sprite_tiles(g,code+frame,2))
s='''local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local frame=0;local test=-1;local terminal=false
local function finish(err)
 terminal=true;local f=assert(io.open(RESULT,"w"));f:write(string.format('{"cases":%d,"error":%s}',test+1,err and string.format('%q',err)or"null"));f:close();manager.machine:exit()
end
local tiles=TILES
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if test>=0 then
   assert(cpu.state["CURPC"].value&0xffffff==0xffb046,"renderer did not return")
   assert(cpu.state["D0"].value&65535==1,"shot missing from SAT")
   assert(m:read_u16(0xffb204)==0x2000+tiles[test//2+1],"incorrect shot tile/flip attributes")
  end
  if test==7 then finish();return end;test=test+1
  for x=ENT,ENT+48*42-1 do m:write_u8(x,0)end
  m:write_u16(GG+88,0);m:write_u8(SURVIVAL,0)
  local a=ENT+16*42;m:write_u8(a,test>=4 and 2 or 1);m:write_u8(a+1,2);m:write_u16(a+2,100);m:write_u16(a+4,100);m:write_u8(a+18,test%2);m:write_u16(a+40,tiles[test//2+1])
  for x=0xffb000,0xffb03e,2 do m:write_u16(x,0x4e71)end
  m:write_u16(0xffb040,0x4eb9);m:write_u32(0xffb042,FN);m:write_u16(0xffb046,0x60fe)
  m:write_u32(0xffbfe0,0xffb200);m:write_u32(0xffbfe4,0)
  cpu.state["SR"].value=0x2700;cpu.state["SP"].value=0xffbfe0;cpu.state["PC"].value=0xffb000
 end)
 if not ok then finish(err)end
end)
'''
for k,v in {'RESULT':json.dumps(str(o/'results.json')),'TILES':'{'+','.join(map(str,tiles))+'}','ENT':syms['g_entities'],'GG':syms['g_game'],'SURVIVAL':syms['g_survivalRun'],'FN':syms['entity_render_all']}.items():s=re.sub(r'\b'+k+r'\b',str(v),s)
(o/'run.lua').write_text(s)
subprocess.run(['mame','genesis','-cart',str(r/'out/rom.bin'),'-window','-nomaximize','-resolution','512x448','-video','none','-keyboardprovider','none','-mouseprovider','none','-joystickprovider','none','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],cwd=r,stdout=(o/'mame.log').open('w'),stderr=subprocess.STDOUT,check=True,timeout=120)
assert hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest()==identity
result=json.loads((o/'results.json').read_text());result.update(rom_sha256=identity,pixel_bytes=512,tiles=tiles);(o/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));assert result['error']is None and result['cases']==8,result
