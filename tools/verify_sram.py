#!/usr/bin/env python3
"""Fresh-process battery-save checks, with explicit score/config fixtures."""
import pathlib,re,json,subprocess,shutil
r=pathlib.Path(__file__).resolve().parents[1];o=r/'out/sram-process-check';o.mkdir(parents=True,exist_ok=True)
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
shutil.rmtree(o/"nvram",ignore_errors=True) # only this test's battery-save directory
# Every process uses the same dedicated SRAM directory and the same ROM identity.
survival_nvram=o/'survival-nvram'
shutil.rmtree(survival_nvram,ignore_errors=True)
shutil.copytree(r/'out/survival-1p-rules/nvram',survival_nvram)
for stage in range(6):
 s='''local m=manager.machine.devices[":maincpu"].spaces.program
local fr=0;local count=0;local failed=0;local f=io.open(RESULT,"w")
local function check(ok,n) count=count+1;if not ok then failed=failed+1 end;f:write((ok and "PASS " or "FAIL ")..n.."\\n");f:flush() end
local function hold(a,b,n) if fr>=a and fr<b then manager.machine.ioport.ports[":ctrl1:mdpad:PAD"].fields["P1 "..n]:set_value(1) end end
emu.register_frame_done(function()
 fr=fr+1;for _,n in ipairs({"Down","Up","Start"}) do manager.machine.ioport.ports[":ctrl1:mdpad:PAD"].fields["P1 "..n]:set_value(0) end
 if STAGE==0 then
  hold(850,862,"Start")
  if fr==950 then m:write_u8(CONFIG,0xDD);m:write_u8(CONFIG+1,0xA9) end
  hold(1000,1012,"Start")
  if fr==1080 then check(m:read_u8(STATUS)==2,"Arcade DIP confirmation writes a validated SRAM record") end
 elseif STAGE==1 then
  if fr==500 then check(m:read_u8(STATUS)==1,"A fresh process loads the previous battery save") end
  hold(900,912,"Down");hold(940,952,"Start");hold(1040,1052,"Up");hold(1080,1092,"Start")
  if fr==1150 then m:write_u8(CONFIG,0xFF);m:write_u8(CONFIG+1,0xE9);m:write_u8(CONFIG+7,9) end
  hold(1200,1212,"Start")
  if fr==1280 then check(m:read_u8(STATUS)==2,"Home Options confirmation writes the next mirrored slot") end
 elseif STAGE==2 or STAGE==3 then
  if fr==500 then check(m:read_u8(STATUS)==1,"New process loads mirrored settings/records") end
  hold(900,912,"Down");hold(940,952,"Start")
  if fr==1000 then check(m:read_u8(CONFIG)==0xFF and m:read_u8(CONFIG+1)==0xE9 and m:read_u8(CONFIG+7)==9,"Home options restore independently from Arcade DIP banks") end
  hold(1040,1052,"Start")
  if STAGE==2 then
   if fr==1100 then m:write_u32(GG+28,123456);m:write_u8(GG+32,1);m:write_u8(GG+38,1);m:write_u8(HC,0) end
   if fr==1350 then check(m:read_u8(STATUS)==2,"Finalized high score commits after the game ends") end
  elseif fr==1100 then check(m:read_u32(BEST)==123456,"A fresh game restores the qualifying high score for its ruleset") end
 elseif STAGE==5 then
  hold(900,912,"Down");hold(940,952,"Start")
  hold(1040,1052,"Down");hold(1080,1092,"Down");hold(1160,1172,"Start")
  if fr==1240 then check(m:read_u8(RUN)==1 and m:read_u32(BESTTICKS)>6000,"Fresh Instadeath game restores its saved survival-time record") end
 else
  hold(850,862,"Start")
  if fr==950 then check(m:read_u8(CONFIG)==0xDD and m:read_u8(CONFIG+1)==0xA9,"Saving Home settings and scores preserved Arcade DIP switches") end
 end
 if fr==1400 then f:write(string.format("CHECKS %d FAILURES %d\\n",count,failed));f:close();manager.machine:exit() end
end)
'''
 vals={'CONFIG':syms['g_config'],'STATUS':syms['g_saveStatus'],'GG':syms['g_game'],'HC':syms['g_homeCredits'],'BEST':syms['run_best_score'],'BESTTICKS':syms['run_best_ticks'],'RUN':syms['g_survivalRun'],'STAGE':stage,'RESULT':json.dumps(str(o/f'stage{stage}.txt'))}
 for k,v in vals.items():s=re.sub(r'\b'+k+r'\b',str(v),s)
 lua=o/f'stage{stage}.lua';lua.write_text(s)
 subprocess.run(['mame','genesis','-cart',str(r/'out/rom.bin'),'-autoboot_script',str(lua),'-window','-skip_gameinfo','-keyboardprovider','none','-mouseprovider','none','-joystickprovider','none','-nothrottle','-video','none','-sound','none','-noreadconfig','-nvram_directory',str(survival_nvram if stage==5 else o/'nvram')],cwd=r,stdout=(o/f'mame{stage}.txt').open('w'),stderr=subprocess.STDOUT,check=True)
 result=(o/f'stage{stage}.txt').read_text();print(result,flush=True);assert 'FAILURES 0' in result and 'PASS ' in result
