#!/usr/bin/env python3
"""Compiled entity loop: generic emitter-before-I/H/G shot integration."""
from pathlib import Path
import hashlib,json,re,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/generic-shot-order';o.mkdir(parents=True,exist_ok=True)
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
rh=hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest()
s='''local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local frame=0;local test=-1;local checks=0;local terminal=false
local function clear(a,n)for k=0,n-1 do m:write_u8(a+k,0)end end
local function check(v,why)checks=checks+1;assert(v,why..' test '..test)end
local function reset()
 m:write_u8(LEGACY,0);clear(KPOOL,64);clear(LARGE,102);clear(ENT,48*42);clear(GG,94);clear(FOES,240);clear(WEAPONS,72);clear(IA,96);clear(IW,10);clear(WAVES,106)
 m:write_u8(ILIVE,0);m:write_u8(IRES,0);m:write_u8(WAVES+4,0);m:write_u8(SURVIVAL,0);m:write_u8(GG+84,2)
 for k=0,1 do m:write_u8(GG+k*42+32,3)end
end
local function run()
 if test==0 then reset();m:write_u8(CLOCK,0);local c=WAVES+10+7*8;m:write_u8(c+2,1);m:write_u8(c+4,2)end
 if test>=64 then
  reset();local owner=(test-64)//2;local last=test%2
  m:write_u8(CLOCK,owner);m:write_u16(DIFFICULTY,0x1cff);m:write_u8(IRES,last==1 and 1 or 2);m:write_u8(ILIVE,1)
  m:write_u16(IA+8,100);m:write_u8(IA+11,136);m:write_u8(IA+13,1);m:write_u8(IA+14,1);m:write_u8(IA+17,3);m:write_u8(IA+21,1)
  local e=ENT+20*42;m:write_u8(e,1);m:write_u8(e+1,10)
  for family=0,1 do
   local a=FOES+family*120;local e=ENT+family*252
   m:write_u16(a,100);m:write_u8(a+3,136);m:write_u8(a+5,1);m:write_u8(a+6,2);m:write_u8(a+7,1);m:write_u8(a+8,80);m:write_u8(a+9,3);m:write_u8(a+14,family==0 and 1 or 10)
   m:write_u8(e,1);m:write_u8(e+1,family==0 and 7 or 1)
  end
  m:write_u8(LEGACY,1) -- Direct RAM fixture bypasses the production allocator.
  local generic=ENT+24*42;m:write_u8(generic,1);m:write_u8(generic+1,12);m:write_u16(generic+2,100);m:write_u16(generic+4,103);m:write_u8(generic+16,90);m:write_u8(generic+17,1)
  local shot=ENT+(16+owner*2)*42;m:write_u8(shot,1);m:write_u8(shot+1,2);m:write_u16(shot+2,94);m:write_u16(shot+4,103);m:write_u8(shot+14,1);m:write_u16(shot+24,94);m:write_u8(shot+27,owner)
 end
 for x=0xffb000,0xffb03e,2 do m:write_u16(x,0x4e71)end
 m:write_u16(0xffb040,0x4eb9);m:write_u32(0xffb042,FN);m:write_u16(0xffb046,0x60fe)
 cpu.state['SR'].value=0x2700;cpu.state['SP'].value=0xffbfe0;cpu.state['PC'].value=0xffb000
end
local function finish(err)
 terminal=true;local f=assert(io.open(RESULT,'w'));f:write(string.format('{"updates":%d,"checks":%d,"error":%s}',test+1,checks,err and string.format('%q',err)or'null'));f:close();manager.machine:exit()
end
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if test>=0 then
   check(cpu.state['CURPC'].value&0xffffff==0xffb046,'return')
   if test<64 then
    local live=0
    for k=0,3 do local birth=1+k*13;local active=test<birth and 0 or test==birth and 128 or 1
     check(m:read_u8(IA+k*24+13)==active,'actor birth slot '..k)
     check(m:read_u8(ENT+(20+k)*42)==active,'entity birth slot '..k)
     if test>birth then live=live+1 end
    end
    check(m:read_u8(ILIVE)==live,'live counter');check(m:read_u8(IRES)==4,'reservation')
   else
    local owner=(test-64)//2;local last=test%2
    check(m:read_u32(GG+owner*42+28)==100,'emitter score')
    check(m:read_u32(GG+(1-owner)*42+28)==0,'other score')
    check(m:read_u8(IA+13)==1,'I survives overlapping emitter')
    check(m:read_u8(ENT+24*42+15)==1,'emitter defeated')
    check(m:read_u8(IRES)==(last==1 and 1 or 2),'I reservation unchanged')
    check(m:read_u8(FOES+5)==1 and m:read_u8(FOES+125)==1,'I before H and G')
    check(m:read_u8(ENT+(16+owner*2)*42)==2,'shot consumed')
   end
  end
  if test==67 then finish();return end;test=test+1;run()
 end)
 if not ok then finish(err)end
end)
'''
for k,v in {'LEGACY':syms['legacy_pool_used'],'KPOOL':syms['arcade_k_actors'],'LARGE':syms['arcade_large_actors'],'ENT':syms['g_entities'],'GG':syms['g_game'],'FOES':syms['arcade_foes'],'WEAPONS':syms['arcade_weapons'],'IA':syms['arcade_i_actors'],'IW':syms['arcade_i_wave'],'WAVES':syms['arcade_waves'],'ILIVE':syms['arcade_i_live'],'IRES':syms['arcade_i_reservation'],'CLOCK':syms['wave_clock'],'SURVIVAL':syms['g_survivalRun'],'DIFFICULTY':syms['g_arcadeCombatDifficulty'],'FN':syms['entity_update_all'],'RESULT':json.dumps(str(o/'results.json'))}.items():s=re.sub(r'\b'+k+r'\b',str(v),s)
(o/'run.lua').write_text(s)
subprocess.run(['mame','genesis','-cart',str(r/'out/rom.bin'),'-window','-nomaximize','-resolution','512x448','-video','none','-keyboardprovider','none','-mouseprovider','none','-joystickprovider','none','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],cwd=r,stdout=(o/'mame.log').open('w'),stderr=subprocess.STDOUT,timeout=120,check=True)
assert hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest()==rh
result=json.loads((o/'results.json').read_text());result['rom_sha256']=rh;(o/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));assert result['error']is None and result['updates']==68,result
