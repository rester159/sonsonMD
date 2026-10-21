#!/usr/bin/env python3
"""Headless target checks for cabinet coins, player join, pause and continues."""
import pathlib,re,json,subprocess,argparse
p=argparse.ArgumentParser();p.add_argument('--p2-only',action='store_true');args=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[1];out=root/'out'/('modes-p2-only' if args.p2_only else 'modes-check');out.mkdir(parents=True,exist_ok=True)
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(root/'out/symbol.txt').read_text(),re.M)}
s='''local m=manager.machine.devices[":maincpu"].spaces.program
local fr=0;local failed=0;local checked=0;local frozen=0;local camera=0
local f=io.open(RESULT,"w")
local function check(ok,n) checked=checked+1;if not ok then failed=failed+1 end;f:write((ok and "PASS " or "FAIL ")..n.."\\n");f:flush() end
local function pad(k,n,v) local p=manager.machine.ioport.ports[":ctrl"..k..":mdpad:PAD"];p.fields["P"..k.." "..n]:set_value(v) end
local function held(a,b,k,n) if fr>=a and fr<b then pad(k,n,1) end end
local function lives(k) return m:read_u8(GG+(k-1)*42+32) end
local function flow(k) return m:read_u8(GG+(k-1)*42+34) end
emu.register_frame_done(function()
 fr=fr+1
 for k=1,2 do for _,n in ipairs({"Start","C","B","Up","Down"}) do pad(k,n,0) end end
 if fr<700 and fr%24<12 then pad(1,"Start",1) end
 held(850,865,1,"Start")
 if fr==900 then check(m:read_u16(LW)==0,"Start without a coin stays on arcade title") end
 held(940,970,1,"C")
 if fr==985 then check(m:read_u8(AC)==1 and m:read_u16(LW)==0,"Held C inserts one coin without starting") end
 held(1000,1012,P2ONLY and 2 or 1,"Start")
 if fr==1030 then
  check(m:read_u16(LW)>0 and m:read_u8(AC)==0,"Paid Start starts game and spends one credit")
  check(P2ONLY and (lives(1)==0 and lives(2)>0) or (lives(1)>0 and lives(2)==0),"Only the requested player starts")
 end
 held(1040,1060,1,"C")
 if fr==1070 then
  local shots=0;for i=0,47 do if m:read_u8(ENT+i*42)~=0 and m:read_u8(ENT+i*42+1)==2 then shots=shots+1 end end
  check(m:read_u8(AC)==1 and shots==0,"Gameplay C adds a coin and does not fire")
 end
 held(1080,1092,P2ONLY and 1 or 2,"Start")
 if fr==1110 then check(lives(1)>0 and lives(2)>0 and m:read_u8(AC)==0 and m:read_u8(PAUSE)==0,"Other controller joins; Arcade Start never pauses") end
 held(1160,1180,2,"C")
 if fr==1190 then check(m:read_u8(AC)==1,"Controller two C inserts one coin") end
 if fr==1200 then m:write_u8(GG+32,0);m:write_u8(GG+34,3) end
 held(1210,1222,1,"Start")
 if fr==1250 then check(lives(1)>0 and m:read_u8(AC)==0,"Arcade Start rejoins an eliminated player") end
 -- Mode injection isolates runtime Home controls; UI Options has a separate fixture.
 if fr==1300 then m:write_u8(MODE,1);m:write_u8(HC,3) end
 held(1350,1380,1,"Start")
 if fr==1360 then frozen=m:read_u32(FC);camera=m:read_u16(GG+88);check(m:read_u8(PAUSE)==1,"Home Start pauses") end
 if fr==1390 then check(m:read_u32(FC)==frozen and m:read_u16(GG+88)==camera,"Held Start does not toggle; simulation and camera freeze") end
 held(1400,1420,2,"Start")
 if fr==1440 then check(m:read_u8(PAUSE)==0 and m:read_u32(FC)>frozen,"Other controller Start resumes") end
 held(1500,1520,1,"C")
 if fr==1530 then check(m:read_u8(HC)==3 and m:read_u8(AC)==0,"Home C does not mint credits") end
 if fr==1550 or fr==1700 then
  for k=0,1 do m:write_u8(GG+k*42+32,0);m:write_u8(GG+k*42+34,3) end
  if fr==1700 then m:write_u8(HC,0) end
 end
 held(1570,1582,1,"Start")
 if fr==1600 then check(m:read_u8(HC)==2 and lives(1)>0 and lives(2)>0,"Home continue spends one shared credit and restores both players") end
 held(1710,1722,2,"Start")
 if fr==1740 then check(m:read_u8(HC)==0 and flow(1)==3 and flow(2)==3,"Zero credits prevents continuing") end
 if fr==985 then manager.machine.video:snapshot() end
 if fr%100==0 then print("MODECHECK",fr,m:read_u8(AC),m:read_u8(MODE),m:read_u16(LW),manager.machine.devices[":maincpu"].state["PC"].value) end
 if fr>=1800 then f:write(string.format("CHECKS %d FAILURES %d\\n",checked,failed));f:close();manager.machine:exit() end
end)
'''
for k,v in {'GG':syms['g_game'],'LW':syms['g_levelW'],'FC':syms['g_frameCounter'],'AC':syms['g_arcadeCredits'],'HC':syms['g_homeCredits'],'PAUSE':syms['g_paused'],'MODE':syms['g_mode'],'ENT':syms['g_entities'],'P2ONLY':'true' if args.p2_only else 'false','RESULT':json.dumps(str(out/'results.txt'))}.items():s=re.sub(r'\b'+k+r'\b',str(v),s)
(out/'run.lua').write_text(s)
subprocess.run(['mame','genesis','-cart',str(root/'out/rom.bin'),'-autoboot_script',str(out/'run.lua'),'-skip_gameinfo','-window','-keyboardprovider','none','-mouseprovider','none','-joystickprovider','none','-nothrottle','-video','none','-sound','none','-noreadconfig','-nvram_directory',str(out/'nvram'),'-snapshot_directory',str(out/'snap')],cwd=root,stdout=(out/'mame.txt').open('w'),stderr=subprocess.STDOUT,check=True)
r=(out/'results.txt').read_text();print(r);assert 'CHECKS 14 FAILURES 0' in r
