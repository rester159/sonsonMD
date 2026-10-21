#!/usr/bin/env python3
"""Measure game ticks per NTSC refresh; symbols must accompany the ROM."""
import argparse,pathlib,re,subprocess,json,hashlib
p=argparse.ArgumentParser();p.add_argument('--rom',default='out/rom.bin');p.add_argument('--players',type=int,choices=[1,2],default=1);p.add_argument('--frames',type=int,default=30000);p.add_argument('--tag',default='perf');a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[1];rom=(root/a.rom).resolve();syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',rom.with_name('symbol.txt').read_text(),re.M)}
out=root/'out'/a.tag;out.mkdir(parents=True,exist_ok=True)
rom_hash=hashlib.sha256(rom.read_bytes()).hexdigest()
symbol_hash=hashlib.sha256(rom.with_name('symbol.txt').read_bytes()).hexdigest()
script='''local mem=manager.machine.devices[":maincpu"].spaces["program"]
local pc=PC;local fc=FC;local lw=LW;local gg=GG;local np=NP;local attr=ATTR
local f=io.open(OUTPUT,"w");f:write("refresh,tick,players,camera,entities,p1x,p2x,present,cpu_load_sum,round,stage\\n")
local frame=0;local samples=0;local started=false
local function pad(k,n,v) local p=manager.machine.ioport.ports[":ctrl"..k..":mdpad:PAD"];if p and p.fields["P"..k.." "..n] then p.fields["P"..k.." "..n]:set_value(v) end end
emu.register_frame_done(function()
 frame=frame+1
 for k=1,2 do for _,n in ipairs({"Start","Right","Left","Up","Down","B","C"}) do pad(k,n,0) end end
 if not started then
  mem:write_u8(np,PLAYERS)
  if frame<740 then
   if frame%24<12 then pad(1,"Start",1) end
  else
   for k=1,PLAYERS do
    if frame%48<8 then pad(k,"C",1) end
    if frame%48>=24 and frame%48<36 then pad(k,"Start",1) end
   end
  end
  if mem:read_u16(lw)>0 and mem:read_u8(attr)==0 and mem:read_u32(fc)>10 then started=true end
 else
  samples=samples+1
  for k=1,PLAYERS do
   mem:write_u8(gg+(k-1)*42+32,99)
   pad(k,"Right",1);if samples%16<3 then pad(k,"B",1) end
   if samples%200<24 then pad(k,"Up",1) elseif samples%200<48 then pad(k,"Down",1) end
  end
  local count=0;for i=0,47 do if mem:read_u8(ENT+i*42)~=0 then count=count+1 end end
  f:write(string.format("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\\n",frame,mem:read_u32(fc),mem:read_u8(gg+84),mem:read_u16(gg+88),count,mem:read_u16(gg),mem:read_u16(gg+42),mem:read_u32(pc),mem:read_u16(CPULOAD),mem:read_u8(gg+85),mem:read_u8(gg+86)))
  if samples==1200 or (samples>=20584 and samples<=20590) then manager.machine.video:snapshot() end
  if samples>=FRAMES then f:close();manager.machine:exit() end
 end
 if frame>FRAMES+6000 then f:close();manager.machine:exit() end
end)
'''
for key,val in {'CPULOAD':syms['cpuFrameLoad'],'PC':syms.get('g_presentCounter',syms['g_frameCounter']),'FC':syms['g_frameCounter'],'LW':syms['g_levelW'],'GG':syms['g_game'],'NP':syms['g_numPlayers'],'ATTR':syms['g_attract'],'ENT':syms['g_entities'],'PLAYERS':a.players,'FRAMES':a.frames,'OUTPUT':json.dumps(str(out/'frames.csv'))}.items():script=re.sub(r'\b'+key+r'\b',str(val),script)
(out/'run.lua').write_text(script)
subprocess.run(['mame','genesis','-cart',str(rom),'-autoboot_script',str(out/'run.lua'),'-skip_gameinfo','-window','-keyboardprovider','none','-mouseprovider','none','-joystickprovider','none','-nothrottle','-video','none','-sound','none','-noreadconfig','-nvram_directory',str(out/'nvram'),'-snapshot_directory',str(out/'snap')],cwd=root,stdout=(out/'mame.txt').open('w'),stderr=subprocess.STDOUT,check=True)
import csv,collections
rows=list(csv.DictReader((out/'frames.csv').open()));ticks=[int(r['tick']) for r in rows];deltas=[b-c for b,c in zip(ticks[1:],ticks)]
assert hashlib.sha256(rom.read_bytes()).hexdigest()==rom_hash, 'ROM changed during measurement'
result={'rom_sha256':rom_hash,'symbols_sha256':symbol_hash,'presentation_counter_available':'g_presentCounter' in syms,'mame_version':subprocess.check_output(['mame','-version'],text=True).strip(),'requested_players':a.players,'observed_players':sorted(set(int(r['players']) for r in rows)),'samples':len(rows),'ticks_per_refresh':(ticks[-1]-ticks[0])/(len(rows)-1) if len(rows)>1 else 0,'tick_delta_histogram':dict(collections.Counter(deltas)),'worst_60_refresh_ticks':min((ticks[i]-ticks[i-60] for i in range(60,len(ticks))),default=0),'max_entities':max((int(r['entities']) for r in rows),default=0)}
present=[int(r['present']) for r in rows];pd=[b-c for b,c in zip(present[1:],present)]
result.update({'presentations_per_refresh':(present[-1]-present[0])/(len(rows)-1),'presentation_delta_histogram':dict(collections.Counter(pd)),'worst_60_refresh_presentations':min(present[i]-present[i-60] for i in range(60,len(present))),'cpu_load_mean_percent':sum(int(r['cpu_load_sum']) for r in rows)/len(rows)*100/2048,'cpu_load_peak_percent':max(int(r['cpu_load_sum']) for r in rows)*100/2048,'rounds_seen':sorted(set(int(r['round']) for r in rows)),'ribbon_wraps':sum(int(b['camera'])<int(c['camera']) for b,c in zip(rows[1:],rows))})
(out/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))

assert len(rows)==a.frames, f'Run ended early: {len(rows)} of {a.frames} refreshes'
assert result['observed_players']==[a.players], 'Wrong game mode'
