#!/usr/bin/env python3
"""Source pixels to linked I tile selector and rendered SAT attributes."""
from pathlib import Path
import hashlib,json,re,struct,subprocess
import gen_assets as G
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/i-render';o.mkdir(parents=True,exist_ok=True)
rom=(r/'out/rom.bin').read_bytes();rh=hashlib.sha256(rom).hexdigest()
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
g=G.Roms(str(r/'arcade'));program=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(program).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
poses=[]
for a in range(0xd50c,0xd528,2):
 for attr in (program[0xd508-0x4000],program[0xd50a-0x4000]):poses.append((program[a-0x4000],attr))
for base in (0xd4cb,0xd4d9,0xd4eb,0xd4f9):
 for k in range(4):a=base+k*3;poses.append(tuple(program[a-0x4000:a-0x4000+2]))
count=int(re.search(r'#define SS_ANIM_TILE_COUNT\s+(\d+)',(r/'inc/ss_anim.h').read_text())[1]);cases=[]
for code,attr in sorted(set(poses)):
 pixels=b''.join(struct.pack('>I',v)for v in G.bake_sprite_tiles(g,code+((attr&32)<<3),attr&31))
 matches=[290+k for k in range(0,count,4)if rom[syms['ss_anim_tiles']+k*32:syms['ss_anim_tiles']+k*32+128]==pixels];assert len(matches)==1,(code,attr,matches)
 cases.append((code,attr,matches[0]));assert attr&128
s='''local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local cases=CASES;local frame=0;local test=0;local phase=0;local terminal=false
local function finish(err)
 terminal=true;local f=assert(io.open(RESULT,'w'));f:write(string.format('{"cases":%d,"error":%s}',test,err and string.format('%q',err)or'null'));f:close();manager.machine:exit()
end
local function invoke(fn,a,b)
 for x=0xffb000,0xffb03e,2 do m:write_u16(x,0x4e71)end
 m:write_u16(0xffb040,0x4eb9);m:write_u32(0xffb042,fn);m:write_u16(0xffb046,0x60fe)
 m:write_u32(0xffbfe0,a);m:write_u32(0xffbfe4,b)
 cpu.state['SR'].value=0x2700;cpu.state['SP'].value=0xffbfe0;cpu.state['PC'].value=0xffb000
end
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if phase>0 then
   assert(cpu.state['CURPC'].value&0xffffff==0xffb046,'return');local c=cases[test+1]
   if phase==1 then
    assert(cpu.state['D0'].value&65535==c[3],'tile selector '..test)
    for x=ENT,ENT+48*42-1 do m:write_u8(x,0)end
    m:write_u16(GG+88,0);m:write_u8(SURVIVAL,0)
    local a=ENT+20*42;m:write_u8(a,test%2+1);m:write_u8(a+1,10);m:write_u16(a+2,100);m:write_u16(a+4,100);m:write_u8(a+18,(c[2]&64)~=0 and 1 or 0);m:write_u16(a+40,c[3])
    phase=2;invoke(RENDER,0xffb200,0);return
   end
   assert(cpu.state['D0'].value&65535==1,'missing sprite')
   assert(m:read_u16(0xffb204)==0x2000+c[3]+((c[2]&64)==0 and 0x800 or 0),'SAT tile/flip')
   test=test+1;phase=0
  end
  if test==#cases then finish();return end
  local c=cases[test+1];phase=1;invoke(TILE,c[1],c[2])
 end)
 if not ok then finish(err)end
end)
'''
for k,v in {'RESULT':json.dumps(str(o/'results.json')),'CASES':'{'+','.join('{'+','.join(map(str,c))+'}'for c in cases)+'}','ENT':syms['g_entities'],'GG':syms['g_game'],'SURVIVAL':syms['g_survivalRun'],'TILE':syms['arcade_i_tile'],'RENDER':syms['entity_render_all']}.items():s=re.sub(r'\b'+k+r'\b',str(v),s)
(o/'run.lua').write_text(s)
subprocess.run(['mame','genesis','-cart',str(r/'out/rom.bin'),'-window','-nomaximize','-resolution','512x448','-video','none','-keyboardprovider','none','-mouseprovider','none','-joystickprovider','none','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],cwd=r,stdout=(o/'mame.log').open('w'),stderr=subprocess.STDOUT,timeout=120,check=True)
assert hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest()==rh
result=json.loads((o/'results.json').read_text());result.update(rom_sha256=rh,pixel_bytes=len(cases)*128);(o/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));assert result['error']is None and result['cases']==len(cases)
