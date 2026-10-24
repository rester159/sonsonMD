#!/usr/bin/env python3
"""Normal fire through game_update does not force a recoil pose."""
from pathlib import Path
import hashlib,json,re,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/fire-effects-integration';o.mkdir(parents=True,exist_ok=True)
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
   check(m:read_u8(p+34)==0,'normal flow')
   check(m:read_u8(p+20)==0,'no invented recoil timer')
   check(m:read_u16(p+26)==(owner==0 and 290 or 322),'normal idle pose while firing')
   check(m:read_u8(ENT+(16+owner*2)*42)==(test//2==1 and 0 or 1),'fire edge gate')
   test=test+1;phase=0;if test==4 then finish();return end
   owner=test%2;lives=test//2+1;p=GG+owner*42;life=LIFE+owner*18
  end
  clear(GG,94);clear(OWN,2);clear(LIFE,36);clear(MOTION,28);clear(ENT,48*42)
  clear(FOES,240);clear(WEAPONS,72);clear(IA,96);clear(IW,10);clear(WAVES,106);clear(JOY,16)
  m:write_u8(SURVIVAL,0);m:write_u8(ATTRACT,0);m:write_u16(RIBBON,0)
  m:write_u8(GG+84,2);m:write_u8(GG+(1-owner)*42+34,3);m:write_u8(CLOCK,owner)
  m:write_u16(p,100);m:write_u16(p+2,103);m:write_u8(p+16,owner);m:write_u8(p+32,lives)
  m:write_u8(MOTION+owner*14+5,0)
  m:write_u8(p+21,owner*4);m:write_u8(p+23,5)
  m:write_u8(AVAILABLE,2);m:write_u8(AVAILABLE+1,2)
  m:write_u16(JOY+owner*2,16);m:write_u8(p+19,test//2)
  phase=1;invoke()
 end)
 if not ok then finish(err)end
end)
'''
for key,name in {'AVAILABLE':'player_shot_available','GG':'g_game','OWN':'life_owned','LIFE':'player_life','MOTION':'player_motion','ENT':'g_entities','FOES':'arcade_foes','WEAPONS':'arcade_weapons','IA':'arcade_i_actors','IW':'arcade_i_wave','WAVES':'arcade_waves','ILIVE':'arcade_i_live','CLOCK':'wave_clock','JOY':'joyState','SURVIVAL':'g_survivalRun','ATTRACT':'g_attract','RIBBON':'g_ribbonW','FN':'game_update'}.items():s=re.sub(r'\b'+key+r'\b',str(syms[name]),s)
s=s.replace('RESULT',json.dumps(str(o/'results.json')));(o/'run.lua').write_text(s);(o/'results.json').unlink(missing_ok=True)
subprocess.run(['mame','genesis','-cart',str(r/'out/rom.bin'),'-window','-nomaximize','-resolution','512x448','-video','none','-keyboardprovider','none','-mouseprovider','none','-joystickprovider','none','-noreadconfig','-autoboot_delay','0','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],stdout=(o/'mame.log').open('w'),stderr=subprocess.STDOUT,check=True,timeout=120)
assert hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest()==rh
result=json.loads((o/'results.json').read_text());result.update(rom_sha256=rh,scope='Normal full game_update firing, both owners, fresh/held fire; no recoil and unchanged normal pose')
(o/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(result);assert result['error']is None and result['cases']==4
