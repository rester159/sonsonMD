#!/usr/bin/env python3
"""Source reentry states through linked game_update, both player owners."""
from pathlib import Path
import csv,hashlib,json,re,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/reentry-integration';o.mkdir(parents=True,exist_ok=True)
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
rh=hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest()
source_report=json.loads((r/'out/parity-audit/reentry-comparison.json').read_text())
assert hashlib.sha256((r/'out/parity-audit/reentry-target.csv').read_bytes()).hexdigest()==source_report['target_sha256']
assert hashlib.sha256((r/'out/parity-audit/reentry-oracle.jsonl').read_bytes()).hexdigest()==source_report['trace_sha256']
rows=[x for x in csv.DictReader((r/'out/parity-audit/reentry-target.csv').open()) if x['stopped']=='0']
oracle=[json.loads(x) for x in (r/'out/parity-audit/reentry-oracle.jsonl').read_text().splitlines() if json.loads(x)['stopped']==0]
assert len(rows)==len(oracle)
for row,original in zip(rows,oracle):row['camera']=original['camera']
rows=[dict(row,teammate=active) for active in (0,1) for row in rows]
trace=o/'cases.csv'
with trace.open('w') as f:
 w=csv.DictWriter(f,fieldnames=rows[0].keys());w.writeheader();w.writerows(rows)
s='''local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local input=assert(io.open(TRACE,'r'));input:read('*l')
local frame=0;local pending=nil;local cases=0;local terminal=false;local owner=0;local releases=0
local function clear(a,n)for k=0,n-1 do m:write_u8(a+k,0)end end
local function bytes(a,s)for k=0,#s//2-1 do m:write_u8(a+k,tonumber(s:sub(k*2+1,k*2+2),16))end end
local function finish(err)
 print("REENTRY_FINISH",cases,err);terminal=true;input:close();local f=assert(io.open(RESULT,'w'));f:write(string.format('{"cases":%d,"animation_handoffs":%d,"error":%s}',cases,releases,err and string.format('%q',err)or'null'));f:close();manager.machine:exit()
end
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value&0xffffff==0xffb046,'return')
   for k=0,16 do assert(m:read_u8(LIFE+owner*18+k)==tonumber(pending[7]:sub(k*2+1,k*2+2),16),'life byte '..k..' case '..cases)end
   local p=GG+owner*42;local kind=tonumber(pending[7]:sub(17,18),16)
   assert(m:read_u8(p+32)==3,'life count '..cases)
   local expected=pending[7]
   assert(m:read_u16(p)==tonumber(expected:sub(1,4),16),'public X '..cases)
   local y=(239-tonumber(expected:sub(7,8),16))&255;if y>=240 then y=y-256 end
   assert(m:read_u16(p+2)==(y&65535),'public Y '..cases)
   assert(m:read_u16(p+4)==tonumber(expected:sub(5,6),16),'public XF '..cases)
   assert(m:read_u16(p+6)==tonumber(expected:sub(9,10),16),'public YF '..cases)
   assert(m:read_u8(p+15)==0,'source sprite flip '..cases)
   local target=WORLD+8+owner*6
   assert(m:read_u16(target)==tonumber(expected:sub(1,4),16),'target X '..cases)
   assert(m:read_u8(target+2)==tonumber(expected:sub(7,8),16),'target Y '..cases)
   assert(m:read_u8(target+3)==tonumber(expected:sub(11,12),16),'target active '..cases..' got '..m:read_u8(target+3)..' expected '..tonumber(expected:sub(11,12),16)..' owned '..m:read_u8(OWN+owner))
   assert(m:read_u8(target+4)==kind,'target kind '..cases)
   assert(m:read_u8(p+34)==(kind==0 and 0 or 2),'flow '..cases)
   assert(m:read_u8(OWN+owner)==(kind==0 and 0 or 1),'owner '..cases)
   if kind==0 then
    local a=ANIMATION+owner*5
    local function b(offset)return tonumber(expected:sub(offset*2+1,offset*2+2),16)end
    local want={owner,b(13)&3,b(14),b(15),b(16)}
    for k=0,4 do assert(m:read_u8(a+k)==want[k+1],'animation handoff byte '..k..' case '..cases)end
    releases=releases+1
   end
   assert(m:read_u8(PATH+owner)==pending[8],'path '..cases)
   assert(m:read_u8(MOTION+owner*14+10)==(pending[5]&60),'history '..cases)
   local shot=ENT+(16+owner*2)*42
   local fired=pending[9]==1 and (pending[5]&1)~=0 and (pending[6]&1)==0
   assert((m:read_u8(shot)~=0)==fired,'shot '..cases)
   cases=cases+1
   if cases%1000==0 then print("REENTRY_PROGRESS",cases)end
  end
  local line=input:read('*l');if not line then finish();return end
  local d={}for n in line:gmatch('[^,]+')do d[#d+1]=n end
  for _,k in ipairs({2,3,4,5,6,8,9,11,12})do d[k]=tonumber(d[k])end;pending=d
  clear(GG,94);clear(OWN,2);bytes(ANIMATION,"a5a5a5a5a5a5a5a5a5a5");clear(MOTION,28);clear(ENT,48*42);clear(WAVES,106);clear(FOES,240);clear(WEAPONS,72);clear(IA,96);clear(IW,10)
  clear(JOY,16);m:write_u8(ATTRACT,0);m:write_u8(SURVIVAL,0);m:write_u16(RIBBON,0)
  owner=tonumber(d[1]:sub(19,20),16)
  m:write_u8(GG+84,2);m:write_u16(GG+88,d[11]);m:write_u8(GG+(1-owner)*42+34,3)
  local p=GG+owner*42;m:write_u8(p+16,owner);m:write_u8(p+32,3);m:write_u8(p+34,2);m:write_u8(p+19,d[6]&1)
  if d[12]==1 then
   local other=1-owner;local q=GG+other*42
   m:write_u8(q+16,other);m:write_u8(q+32,3);m:write_u8(q+34,0)
   m:write_u16(q,d[11]+100);m:write_u16(q+2,199)
   m:write_u8(MOTION+other*14+5,1)
  end
  bytes(LIFE+owner*18,d[1]);m:write_u8(OWN+owner,1)
  m:write_u8(PATH+owner,d[2]);m:write_u8(CLOCK,d[3]);m:write_u8(MOTION+owner*14+10,d[6]&60)
  m:write_u8(AVAILABLE,2);m:write_u8(AVAILABLE+1,2)
  local pad=(d[5]&12)|((d[5]&16)>>4)|((d[5]&32)>>4)|((d[5]&1)<<4)
  m:write_u16(JOY+owner*2,pad)
  for x=0xffb000,0xffb03e,2 do m:write_u16(x,0x4e71)end
  m:write_u16(0xffb040,0x4eb9);m:write_u32(0xffb042,FN);m:write_u16(0xffb046,0x60fe)
  cpu.state['SR'].value=0x2700;cpu.state['SP'].value=0xffbfe0;cpu.state['PC'].value=0xffb000
 end)
 if not ok then finish(err)end
end)
'''
for key,name in {'ANIMATION':'player_animation','WORLD':'arcade_world','PATH':'life_path','CLOCK':'wave_clock','AVAILABLE':'player_shot_available','GG':'g_game','LIFE':'player_life','OWN':'life_owned','MOTION':'player_motion','ENT':'g_entities','WAVES':'arcade_waves','FOES':'arcade_foes','WEAPONS':'arcade_weapons','IA':'arcade_i_actors','IW':'arcade_i_wave','JOY':'joyState','ATTRACT':'g_attract','SURVIVAL':'g_survivalRun','RIBBON':'g_ribbonW','FN':'game_update'}.items():s=re.sub(r'\b'+key+r'\b',str(syms[name]),s)
for key,value in {'TRACE':str(trace),'RESULT':str(o/'results.json')}.items():s=re.sub(r'\b'+key+r'\b',json.dumps(value),s)
(o/'run.lua').write_text(s)
(o/'results.json').unlink(missing_ok=True)
subprocess.run(['mame','genesis','-cart',str(r/'out/rom.bin'),'-window','-nomaximize','-resolution','512x448','-video','none','-keyboardprovider','none','-mouseprovider','none','-joystickprovider','none','-noreadconfig','-autoboot_delay','0','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],cwd=r,stdout=(o/'mame.log').open('w'),stderr=subprocess.STDOUT,timeout=300,check=True)
assert hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest()==rh
result=json.loads((o/'results.json').read_text());result.update(rom_sha256=rh,scope='Injected source reentry states through full game_update; moving camera; teammate active/out; life/path/input/flow, public coordinates/flip, source animation handoff and shot presence; contact reachability and SAT separate')
(o/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(result);assert result['error']is None and result['cases']==len(rows) and result['animation_handoffs']>0
