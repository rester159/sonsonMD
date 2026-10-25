#!/usr/bin/env python3
"""Replay original two-player MC6809 states on the real compiled 68000 ROM.
Input-read taps isolate timing from different board/controller scan hardware.
Only the initial pose and invulnerable flow are seeded; physics runs normally.
"""
from pathlib import Path
import csv,re,subprocess,json,hashlib
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/target-player';o.mkdir(parents=True,exist_ok=True)
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
rows=list(csv.DictReader((r/'out/parity-audit/arcade-2p.csv').open()));assert len(rows)==1281
keys=['x','xf','y','yf','state','phase','face','attached','held','timer','stamp']
expect=[]
for row in rows:expect.append('{'+','.join('{'+','.join(row[f'p{k}_{key}']for key in keys)+'}'for k in [1,2])+'}')
s='local expected={\n'+',\n'.join(expect)+'\n}\n'+'''
local m=manager.machine.devices[":maincpu"].spaces.program
local fr=0;local index=0;local tap=nil;local complete=nil;local installed=false;local checks=0;local terminal=false
local f=assert(io.open(RESULT,"w"))
local function pad(n,v)manager.machine.ioport.ports[":ctrl1:mdpad:PAD"].fields["P1 "..n]:set_value(v)end
local function finish(errors)
 if terminal then return end;terminal=true
 f:write(string.format('{"frames_compared":%d,"field_checks":%d,"mismatches":%s}',index-1,checks,errors or "[]"));f:close();manager.machine:exit()
end
local function seed()
 local initial=expected[1]
 for k=0,1 do
  local p=GG+k*42;local q=MOTION+k*14;local a=initial[k+1]
  m:write_u16(p,a[1]);m:write_u16(p+2,(239-a[3])&65535);m:write_u16(p+4,a[2]);m:write_u16(p+6,a[4])
  m:write_u8(p+15,a[7]==3 and 1 or 0);m:write_u8(p+16,k);m:write_u8(p+32,99);m:write_u8(p+34,2);m:write_u16(p+36,60000)
  m:write_u16(q,a[1]);m:write_u8(q+2,a[2]);m:write_u8(q+3,a[3]);m:write_u8(q+4,a[4]);m:write_u8(q+5,a[5]);m:write_u8(q+6,a[6]);m:write_u8(q+7,a[10]);m:write_u8(q+8,a[11]);m:write_u8(q+9,a[7]);m:write_u8(q+10,a[9]);m:write_u8(q+11,1);m:write_u8(q+12,a[8])
 end
 m:write_u16(GG+88,0);m:write_u16(SUB,141)
 tap=m:install_read_tap(JOY,JOY+3,"source-controller-replay",function(offset,data,mask)
  local pos=offset>=JOY and offset-JOY or offset;local k=(pos//2)%2+1
  local a=expected[index+1] or expected[#expected];local held=a[k][9]

  return (held&12)|((held>>4)&3)
 end)
 index=1
end
emu.register_frame_done(function()
 fr=fr+1
 for _,n in ipairs({"Start","Down"})do pad(n,0)end
 if fr>=920 and fr<932 or fr>=1040 and fr<1052 then pad("Down",1)end
 if fr>=950 and fr<965 or fr>=1100 and fr<1112 then pad("Start",1)end
 if terminal then manager.machine:exit();return end
 if not installed and fr>=1200 and m:read_u16(LW)>0 and m:read_u8(GG+84)==2 then
  installed=true
  complete=m:install_write_tap(PC+2,PC+3,"completed-presentation",function(offset,data,mask)
   if terminal then return end
   if index==0 then seed();return end

   local a=expected[index+1]
   for k=0,1 do
    local p=GG+k*42;local q=MOTION+k*14
    local y=m:read_u16(p+2);if y>=32768 then y=y-65536 end
    local actual={m:read_u16(p),m:read_u16(p+4),(239-y)&255,m:read_u16(p+6),m:read_u8(q+5),m:read_u8(q+6),m:read_u8(p+15)==1 and 3 or 1,m:read_u8(q+12)}
    for key=1,8 do
     checks=checks+1
     if actual[key]~=a[k+1][key] then
      
      
      finish(string.format('[{"tick":%d,"player":%d,"field":%d,"native":%d,"arcade":%d}]',index,k+1,key,actual[key],a[k+1][key]));return
     end
    end
   end
   index=index+1
   if index>=#expected then finish();return end
  end)


 end
 if fr>4500 then finish('[{"error":"fixture timeout"}]')end
end)
'''
for key,value in {'GG':syms['g_game'],'MOTION':syms['player_motion'],'SUB':syms['g_scrollSub'],'FC':syms['g_frameCounter'],'PC':syms['g_presentCounter'],'JOY':syms['joyState'],'LW':syms['g_levelW'],'RESULT':json.dumps(str(o/'results.json'))}.items():s=re.sub(r'\b'+key+r'\b',str(value),s)
(o/'run.lua').write_text(s)
subprocess.run(['mame','genesis','-cart',str(r/'out/rom.bin'),'-autoboot_script',str(o/'run.lua'),'-window','-nomaximize','-resolution','512x448','-video','none','-keyboardprovider','none','-mouseprovider','none','-joystickprovider','none','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-nvram_directory',str(o/'nvram'),'-snapshot_directory',str(o/'snap')],cwd=r,stdout=(o/'mame.txt').open('w'),stderr=subprocess.STDOUT,check=True)
result=json.loads((o/'results.json').read_text());result['rom_sha256']=hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest();(o/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(result);assert result['frames_compared']==1280 and not result['mismatches']
