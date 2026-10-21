#!/usr/bin/env python3
"""Controller-driven Instadeath checks; optional explicitly injected maximum load."""
import pathlib,re,json,subprocess,argparse,csv,collections,shutil
p=argparse.ArgumentParser();p.add_argument('--players',type=int,choices=[1,2],default=1);p.add_argument('--stress',action='store_true');p.add_argument('--frames',type=int,default=6000);a=p.parse_args()
r=pathlib.Path(__file__).resolve().parents[1]
o=r/'out'/f'survival-{a.players}p-{ "stress" if a.stress else "rules"}'
o.mkdir(parents=True,exist_ok=True)
shutil.rmtree(o/"nvram",ignore_errors=True) # dedicated disposable fixture saves
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
s='''local m=manager.machine.devices[":maincpu"].spaces.program
local fr=0;local failed=0;local checked=0;local freeze=0;local stopped=0;local frames=0
local f=io.open(RESULT,"w");local trace=io.open(TRACE,"w");trace:write("refresh,tick,present,entities,scheduled,cpu\\n")
local function check(ok,n) checked=checked+1;if not ok then failed=failed+1 end;f:write((ok and "PASS " or "FAIL ")..n.."\\n");f:flush() end
local function pad(k,n,v) manager.machine.ioport.ports[":ctrl"..k..":mdpad:PAD"].fields["P"..k.." "..n]:set_value(v) end
local function hold(a,b,k,n) if fr>=a and fr<b then pad(k,n,1) end end
emu.register_frame_done(function()
 fr=fr+1;for k=1,2 do for _,n in ipairs({"Start","Down","Up","Right","B","C"}) do pad(k,n,0) end end
 hold(900,912,1,"Down");hold(940,952,1,"Start")
 hold(1040,1052,1,"Down");hold(1080,1092,1,"Down");if PLAYERS==2 then hold(1120,1132,1,"Down") end
 hold(1160,1190,1,"Start")
 if STRESS and fr>=1200 then
  m:write_u8(PRESSURE,6)
  for k=0,PLAYERS-1 do
   m:write_u8(GG+k*42+34,2);m:write_u16(GG+k*42+36,60000)
   pad(k+1,"Right",1);if fr%16<3 then pad(k+1,"B",1) end
  end
  if fr==3600 then manager.machine.video:snapshot() end
  if fr>=2200 then
   frames=frames+1;local n=0;local sched=0
   for k=0,47 do if m:read_u8(ENT+k*42)~=0 then n=n+1;if m:read_u8(ENT+k*42+29)~=0 then sched=sched+1 end end end
   trace:write(string.format("%d,%d,%d,%d,%d,%d\\n",fr,m:read_u32(FC),m:read_u32(PC),n,sched,m:read_u16(CPU)))
   if frames>=FRAMES then trace:close();f:close();manager.machine:exit() end
  end
 elseif not STRESS then
  if fr==1220 then
   check(m:read_u8(RUN)==1 and m:read_u8(GG+84)==PLAYERS,"Home Instadeath menu starts requested player count")
   check(m:read_u8(HC)==0 and m:read_u8(GG+32)==1,"One life and no continue credits")
   m:write_u32(GG+28,50000)
  end
  if fr==1240 then check(m:read_u8(GG+32)==1,"Scoring cannot award bonus lives") end
  hold(1260,1290,1,"Start")
  if fr==1270 then freeze=m:read_u32(TICKS);check(m:read_u8(PAUSE)==1,"Home Start pauses Instadeath") end
  if fr==1320 then check(m:read_u32(TICKS)==freeze,"Survival timer freezes while paused") end
  hold(1340,1352,1,"Start")
  if fr==1380 then check(m:read_u8(PAUSE)==0 and m:read_u32(TICKS)>freeze,"Timer resumes with gameplay") end
  if fr==1390 then m:write_u32(TICKS,5700);m:write_u32(SECONDS,19) end
  if fr==1500 then check(m:read_u8(PRESSURE)==1,"Enemy pressure increases at twenty seconds");manager.machine.video:snapshot() end
  if fr==1510 then
   for k=0,PLAYERS-1 do
    for slot=0,47 do
     local e=ENT+slot*42
     if m:read_u8(e)==0 then
      for b=0,41 do m:write_u8(e+b,0) end
      m:write_u8(e,1);m:write_u8(e+1,1);m:write_u8(e+17,1)
      m:write_u16(e+2,m:read_u16(GG+k*42));m:write_u16(e+4,m:read_u16(GG+k*42+2))
      break
     end
    end
   end
  end
  if fr==1520 then
   stopped=m:read_u32(TICKS)
   local dead=true;for k=0,PLAYERS-1 do if m:read_u8(GG+k*42+32)~=0 or m:read_u8(GG+k*42+34)~=3 then dead=false end end
   check(dead,"One lethal hit ends each player's run immediately")
  end
  hold(1540,1552,1,"Start")
  if fr==1580 then check(m:read_u32(TICKS)==stopped and m:read_u8(GG+34)==3,"Final timer stops and Start cannot continue") end
  if fr==1760 then check(m:read_u8(STATUS)==2,"Survival record committed to SRAM");f:write(string.format("CHECKS %d FAILURES %d\\n",checked,failed));f:close();trace:close();manager.machine:exit() end
 end
 if fr>FRAMES+7000 then f:close();trace:close();manager.machine:exit() end
end)
'''
vals={**{k:syms[v] for k,v in {'GG':'g_game','RUN':'g_survivalRun','PRESSURE':'g_survivalPressure','TICKS':'g_survivalTicks','SECONDS':'g_survivalSeconds','HC':'g_homeCredits','PAUSE':'g_paused','FC':'g_frameCounter','PC':'g_presentCounter','ENT':'g_entities','CPU':'cpuFrameLoad','STATUS':'g_saveStatus'}.items()},'STRESS':'true' if a.stress else 'false','PLAYERS':a.players,'FRAMES':a.frames,'RESULT':json.dumps(str(o/'results.txt')),'TRACE':json.dumps(str(o/'frames.csv'))}
for k,v in vals.items():s=re.sub(r'\b'+k+r'\b',str(v),s)
(o/'run.lua').write_text(s)
subprocess.run(['mame','genesis','-cart',str(r/'out/rom.bin'),'-autoboot_script',str(o/'run.lua'),'-window','-skip_gameinfo','-keyboardprovider','none','-mouseprovider','none','-joystickprovider','none','-nothrottle','-video','none','-sound','none','-noreadconfig','-nvram_directory',str(o/'nvram'),'-snapshot_directory',str(o/'snap')],cwd=r,stdout=(o/'mame.txt').open('w'),stderr=subprocess.STDOUT,check=True)
if not a.stress:
 text=(o/'results.txt').read_text();print(text);assert 'CHECKS 10 FAILURES 0' in text
else:
 rows=list(csv.DictReader((o/'frames.csv').open()));ps=[int(v['present']) for v in rows];d=dict(collections.Counter(b-c for c,b in zip(ps,ps[1:])))
 result={'players':a.players,'samples':len(rows),'presentation_deltas':d,'cpu_peak_percent':max(int(v['cpu']) for v in rows)*100/2048,'max_entities':max(int(v['entities']) for v in rows),'max_scheduled':max(int(v['scheduled']) for v in rows)}
 (o/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(result);assert len(rows)==a.frames and d=={1:a.frames-1}
