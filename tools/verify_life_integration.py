#!/usr/bin/env python3
"""Source death states through linked game_update, both player owners."""
from pathlib import Path
import csv,hashlib,json,re,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/life-integration';o.mkdir(parents=True,exist_ok=True)
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
rh=hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest()
rows=[x for x in csv.DictReader((r/'out/parity-audit/life-target.csv').open()) if x['stopped']=='0' and x['reserves']!='255']
trace=o/'cases.csv'
with trace.open('w') as f:
 w=csv.DictWriter(f,fieldnames=rows[0].keys());w.writeheader();w.writerows(rows)
s='''local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local input=assert(io.open(TRACE,'r'));input:read('*l')
local frame=0;local pending=nil;local cases=0;local terminal=false;local owner=0
local function clear(a,n)for k=0,n-1 do m:write_u8(a+k,0)end end
local function bytes(a,s)for k=0,#s//2-1 do m:write_u8(a+k,tonumber(s:sub(k*2+1,k*2+2),16))end end
local function finish(err)
 terminal=true;input:close();local f=assert(io.open(RESULT,'w'));f:write(string.format('{"cases":%d,"error":%s}',cases,err and string.format('%q',err)or'null'));f:close();manager.machine:exit()
end
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value&0xffffff==0xffb046,'return')
   for k=0,16 do assert(m:read_u8(LIFE+owner*18+k)==tonumber(pending[6]:sub(k*2+1,k*2+2),16),'life byte '..k..' case '..cases)end
   local p=GG+owner*42
   assert(m:read_u8(p+32)==(pending[8]==1 and 0 or pending[7]+1),'life count '..cases)
   do
    local target=WORLD+8+owner*6
    assert(m:read_u8(target+3)==tonumber(pending[6]:sub(11,12),16),'target active '..cases)
    assert(m:read_u8(target+4)==tonumber(pending[6]:sub(17,18),16),'target kind '..cases)
   end
   assert(m:read_u8(p+34)==(pending[8]==1 and 3 or tonumber(pending[6]:sub(13,14),16)==7 and 2 or 1),'flow '..cases)
   cases=cases+1
  end
  local line=input:read('*l');if not line then finish();return end
  local d={}for n in line:gmatch('[^,]+')do d[#d+1]=n end
  for _,k in ipairs({2,3,4,5,7,8,9,10})do d[k]=tonumber(d[k])end;pending=d
  clear(GG,94);clear(OWN,2);clear(MOTION,28);clear(ENT,48*42);clear(WAVES,106);clear(FOES,240);clear(WEAPONS,72);clear(IA,96);clear(IW,10)
  clear(JOY,16);m:write_u8(ATTRACT,0);m:write_u8(SURVIVAL,0);m:write_u16(RIBBON,0)
  owner=tonumber(d[1]:sub(19,20),16)
  m:write_u8(GG+42+16,1);m:write_u8(GG+84,2);m:write_u16(GG+88,d[2]);m:write_u8(GG+(1-owner)*42+34,3)
  local p=GG+owner*42;m:write_u8(p+16,owner);m:write_u8(p+32,d[4]+1);m:write_u8(p+34,1)
  bytes(LIFE+owner*18,d[1]);m:write_u8(OWN+owner,1)
  for x=0xffb000,0xffb03e,2 do m:write_u16(x,0x4e71)end
  m:write_u16(0xffb040,0x4eb9);m:write_u32(0xffb042,FN);m:write_u16(0xffb046,0x60fe)
  cpu.state['SR'].value=0x2700;cpu.state['SP'].value=0xffbfe0;cpu.state['PC'].value=0xffb000
 end)
 if not ok then finish(err)end
end)
'''
for key,name in {'WORLD':'arcade_world','GG':'g_game','LIFE':'player_life','OWN':'life_owned','MOTION':'player_motion','ENT':'g_entities','WAVES':'arcade_waves','FOES':'arcade_foes','WEAPONS':'arcade_weapons','IA':'arcade_i_actors','IW':'arcade_i_wave','JOY':'joyState','ATTRACT':'g_attract','SURVIVAL':'g_survivalRun','RIBBON':'g_ribbonW','FN':'game_update'}.items():s=re.sub(r'\b'+key+r'\b',str(syms[name]),s)
for key,value in {'TRACE':str(trace),'RESULT':str(o/'results.json')}.items():s=re.sub(r'\b'+key+r'\b',json.dumps(value),s)
(o/'run.lua').write_text(s)
(o/'results.json').unlink(missing_ok=True)
subprocess.run(['mame','genesis','-cart',str(r/'out/rom.bin'),'-window','-nomaximize','-resolution','512x448','-video','none','-keyboardprovider','none','-mouseprovider','none','-joystickprovider','none','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_delay','0','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],cwd=r,stdout=(o/'mame.log').open('w'),stderr=subprocess.STDOUT,timeout=300,check=True)
assert hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest()==rh
result=json.loads((o/'results.json').read_text());result.update(rom_sha256=rh,scope='Injected source death states through full game_update; moving camera, reserves0..2; source contact/start/reentry reachability separate')
(o/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(result);assert result['error']is None and result['cases']==len(rows)
