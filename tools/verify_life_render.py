#!/usr/bin/env python3
"""Source death poses through full game_update and game_render SAT preparation."""
from pathlib import Path
import csv,hashlib,json,re,struct,subprocess
import gen_assets as G
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/life-render';o.mkdir(parents=True,exist_ok=True)
rom=(r/'out/rom.bin').read_bytes();rh=hashlib.sha256(rom).hexdigest()
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+([\w.]+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
g=G.Roms(str(r/'arcade'));unique={}
for row in csv.DictReader((r/'out/parity-audit/life-target.csv').open()):
 b=bytes.fromhex(row['after_life'])
 if row['stopped']!='0' or row['after_gate']!='0' or row['reserves']=='255':continue
 key=(b[15],b[16]);unique.setdefault(key,row)
count=int(re.search(r'#define SS_ANIM_TILE_COUNT\s+(\d+)',(r/'inc/ss_anim.h').read_text())[1])
cases=[]
for (code,attr),row in sorted(unique.items()):
 pixel=b''.join(struct.pack('>I',v) for v in G.bake_sprite_tiles(g,code,attr&31))
 tiles=[290+k for k in range(0,count,4) if rom[syms['ss_anim_tiles']+k*32:syms['ss_anim_tiles']+k*32+128]==pixel];assert len(tiles)==1
 for timer in (0,255):cases.append('{'+','.join([json.dumps(row['life']),row['camera'],row['reserves'],json.dumps(row['after_life']),str(tiles[0]),str(timer)])+'}')
s='''local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local cases=CASES;local test=0;local phase=0;local frame=0;local terminal=false
local function clear(a,n)for k=0,n-1 do m:write_u8(a+k,0)end end
local function bytes(a,s)for k=0,#s//2-1 do m:write_u8(a+k,tonumber(s:sub(k*2+1,k*2+2),16))end end
local function invoke(fn)
 for x=0xffb000,0xffb03e,2 do m:write_u16(x,0x4e71)end
 m:write_u16(0xffb040,0x4eb9);m:write_u32(0xffb042,fn);m:write_u16(0xffb046,0x60fe)
 cpu.state['SR'].value=0x2700;cpu.state['SP'].value=0xffbfe0;cpu.state['PC'].value=0xffb000
end
local function finish(err)
 terminal=true;local f=assert(io.open(RESULT,'w'));f:write(string.format('{"cases":%d,"error":%s}',test,err and string.format('%q',err)or'null'));f:close();manager.machine:exit()
end
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if phase>0 then
   assert(cpu.state['CURPC'].value&0xffffff==0xffb046,'return')
   local c=cases[test+1];local owner=tonumber(c[1]:sub(19,20),16);local p=GG+owner*42
   if phase==1 then
    assert(m:read_u16(p+26)==c[5],'source tile '..test)
    m:write_u16(GG+88,0);m:write_u16(GG+90,0);m:write_u16(GG+92,63)
    m:write_u16(QINDEX,0);m:write_u16(QLIMIT,0);m:write_u16(QSIZE,0)
    phase=3;invoke(RENDER);return end
   local sat=SAT+owner*8;local y=(239-tonumber(c[4]:sub(7,8),16))&255
   if y>=240 then y=y-256 end
   assert(m:read_u16(sat)==((128+y)&1023),'Y/blink '..test)
   assert(m:read_u16(sat+6)==((128+tonumber(c[4]:sub(1,4),16))&511),'X '..test)
   assert(m:read_u16(sat+4)==0x2000+c[5],'tile/palette/flip '..test)
   assert(m:read_u16(sat+2)==0x500+(owner==0 and 1 or 0),'size/link '..test)
   test=test+1;phase=0
  end
  if test==#cases then finish();return end
  local c=cases[test+1];local owner=tonumber(c[1]:sub(19,20),16);local p=GG+owner*42
  clear(GG,94);clear(OWN,2);clear(MOTION,28);clear(ENT,48*42);clear(WAVES,106);clear(FOES,240);clear(WEAPONS,72);clear(IA,96);clear(IW,10);clear(JOY,16)
  m:write_u8(ATTRACT,0);m:write_u8(SURVIVAL,0);m:write_u16(RIBBON,0)
  m:write_u8(GG+84,2);m:write_u16(GG+88,c[2]);m:write_u8(GG+(1-owner)*42+34,3)
  m:write_u8(p+16,owner);m:write_u8(p+32,c[3]+1);m:write_u8(p+34,1);m:write_u16(p+36,c[6])
  bytes(LIFE+owner*18,c[1]);m:write_u8(OWN+owner,1);phase=1;invoke(UPDATE)
 end)
 if not ok then finish(err)end
end)
'''
for key,name in {'GG':'g_game','OWN':'life_owned','LIFE':'player_life','MOTION':'player_motion','ENT':'g_entities','FOES':'arcade_foes','WEAPONS':'arcade_weapons','IA':'arcade_i_actors','IW':'arcade_i_wave','WAVES':'arcade_waves','JOY':'joyState','SURVIVAL':'g_survivalRun','ATTRACT':'g_attract','RIBBON':'g_ribbonW','UPDATE':'game_update','RENDER':'game_render','QINDEX':'queueIndex','QLIMIT':'queueIndexLimit','QSIZE':'queueTransferSize','SAT':'sat.0'}.items():s=re.sub(r'\b'+key+r'\b',str(syms[name]),s)
s=s.replace('CASES','{'+','.join(cases)+'}').replace('RESULT',json.dumps(str(o/'results.json')))
(o/'run.lua').write_text(s);(o/'results.json').unlink(missing_ok=True)
subprocess.run(['mame','genesis','-cart',str(r/'out/rom.bin'),'-window','-nomaximize','-resolution','512x448','-video','none','-keyboardprovider','none','-mouseprovider','none','-joystickprovider','none','-noreadconfig','-autoboot_delay','0','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],stdout=(o/'mame.log').open('w'),stderr=subprocess.STDOUT,check=True,timeout=120)
assert hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest()==rh
result=json.loads((o/'results.json').read_text());result.update(rom_sha256=rh,poses=len(unique),scope='Source death/idle/entry tile pixels and linked update/render SAT buffer; two legacy blink timer values; VDP transfer not checked')
(o/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(result);assert result['error']is None and result['cases']==len(cases)
