#!/usr/bin/env python3
"""Actual class-I contact through game_update into source-owned death next tick."""
from pathlib import Path
import hashlib,json,re,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/contact-life';o.mkdir(parents=True,exist_ok=True)
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
rh=hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest()
s='''local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local frame=0;local test=0;local phase=0;local checks=0;local terminal=false
local function clear(a,n)for k=0,n-1 do m:write_u8(a+k,0)end end
local function check(v,msg)checks=checks+1;assert(v,msg..' case '..test)end
local function invoke()
 for x=0xffb000,0xffb03e,2 do m:write_u16(x,0x4e71)end
 m:write_u16(0xffb040,0x4eb9);m:write_u32(0xffb042,FN);m:write_u16(0xffb046,0x60fe)
 cpu.state['SR'].value=0x2700;cpu.state['SP'].value=0xffbfe0;cpu.state['PC'].value=0xffb000
end
local function finish(err)
 terminal=true;local f=assert(io.open(RESULT,'w'));f:write(string.format('{"cases":%d,"checks":%d,"error":%s}',test,checks,err and string.format('%q',err)or'null'));f:close();manager.machine:exit()
end
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  local owner=test%2;local lives=test//2+1;local p=GG+owner*42;local life=LIFE+owner*18
  if phase>0 then
   check(cpu.state['CURPC'].value&0xffffff==0xffb046,'return')
   check(m:read_u8(p+34)==1,'dying flow even on final life')
   check(m:read_u8(p+32)==lives,'reserve decrement deferred')
   check(m:read_u8(p+38)==0,'request consumed')
   check(m:read_u8(OWN+owner)==1,'life owner')
   check(m:read_u8(life+6)==0,'death state0')
   if phase==1 then
    check(m:read_u8(life+7)==0,'death not executed at contact')
    check(m:read_u8(life+5)==1,'source pre-death active')
    clear(ENT,48*42);clear(IA,96);clear(WAVES,106);phase=2;invoke();return
   end
   check(m:read_u8(life+7)==1,'death executes next tick')
   check(m:read_u8(life+5)==2,'source death active')
   check(m:read_u8(life+11)==59,'first death tick decrements60')
   check(m:read_u8(life+15)==(owner==0 and 0x42 or 0x4a),'original first death pose')
   test=test+1;phase=0;if test==6 then finish();return end
   owner=test%2;lives=test//2+1;p=GG+owner*42;life=LIFE+owner*18
  end
  clear(GG,94);clear(OWN,2);clear(LIFE,36);clear(MOTION,28);clear(ENT,48*42)
  clear(FOES,240);clear(WEAPONS,72);clear(IA,96);clear(IW,10);clear(WAVES,106);clear(JOY,16)
  m:write_u8(SURVIVAL,0);m:write_u8(ATTRACT,0);m:write_u16(RIBBON,0)
  m:write_u8(GG+84,2);m:write_u8(GG+(1-owner)*42+34,3);m:write_u8(CLOCK,owner)
  m:write_u16(p,100);m:write_u16(p+2,103);m:write_u8(p+16,owner);m:write_u8(p+32,lives)
  m:write_u8(MOTION+owner*14+5,0)
  m:write_u8(ILIVE,1);m:write_u16(IA+8,100);m:write_u8(IA+11,136)
  m:write_u8(IA+13,1);m:write_u8(IA+14,1);m:write_u8(IA+17,3);m:write_u8(IA+21,1)
  local e=ENT+20*42;m:write_u8(e,1);m:write_u8(e+1,10);m:write_u8(e+27,128)
  phase=1;invoke()
 end)
 if not ok then finish(err)end
end)
'''
for key,name in {'GG':'g_game','OWN':'life_owned','LIFE':'player_life','MOTION':'player_motion','ENT':'g_entities','FOES':'arcade_foes','WEAPONS':'arcade_weapons','IA':'arcade_i_actors','IW':'arcade_i_wave','WAVES':'arcade_waves','ILIVE':'arcade_i_live','CLOCK':'wave_clock','JOY':'joyState','SURVIVAL':'g_survivalRun','ATTRACT':'g_attract','RIBBON':'g_ribbonW','FN':'game_update'}.items():s=re.sub(r'\b'+key+r'\b',str(syms[name]),s)
s=s.replace('RESULT',json.dumps(str(o/'results.json')));(o/'run.lua').write_text(s);(o/'results.json').unlink(missing_ok=True)
subprocess.run(['mame','genesis','-cart',str(r/'out/rom.bin'),'-window','-nomaximize','-resolution','512x448','-video','none','-keyboardprovider','none','-mouseprovider','none','-joystickprovider','none','-noreadconfig','-autoboot_delay','0','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],stdout=(o/'mame.log').open('w'),stderr=subprocess.STDOUT,check=True,timeout=120)
assert hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest()==rh
result=json.loads((o/'results.json').read_text());result.update(rom_sha256=rh,scope='Seeded overlapping class-I enemy, full game_update contact and subsequent death tick; both owners,lives1..3')
(o/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(result);assert result['error']is None and result['cases']==6
