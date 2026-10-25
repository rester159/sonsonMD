#!/usr/bin/env python3
"""Compare completed native gameplay updates before/after a CPU optimization.
Input is keyed to simulation index, not refresh, so a missed VBlank cannot change
its meaning. Whole entity/player state is captured only at completed updates.
"""
from pathlib import Path
import argparse,hashlib,json,re,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--baseline',type=Path,required=True);p.add_argument('--frames',type=int,default=4000);a=p.parse_args()
r=Path(__file__).resolve().parents[1];out=r/'out/parity-audit/workload-comparison';out.mkdir(parents=True,exist_ok=True)
for name,rom,symbols in [('before',a.baseline/'rom.bin',a.baseline/'symbol.txt'),('after',r/'out/rom.bin',r/'out/symbol.txt')]:
 syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',symbols.read_text(),re.M)}
 s='''local m=manager.machine.devices[":maincpu"].spaces.program
local frame=0;local index=0;local installed=false;local terminal=false;local joy=nil;local complete=nil
local f=assert(io.open(OUTPUT,"w"))
local function pad(k,n,v)manager.machine.ioport.ports[":ctrl"..k..":mdpad:PAD"].fields["P"..k.." "..n]:set_value(v)end
local function hex(addr,n)local v={}for k=0,n-1 do v[#v+1]=string.format("%02x",m:read_u8(addr+k))end;return table.concat(v)end
emu.register_frame_done(function()
 frame=frame+1
 if terminal then manager.machine:exit();return end
 for k=1,2 do for _,n in ipairs({"Start","C","B","Up","Down"})do pad(k,n,0)end end
 if frame<700 and frame%24<12 then pad(1,"Start",1)end
 if frame>=900 and frame<916 then pad(1,"C",1)end
 if frame>=940 and frame<956 then pad(1,"Start",1)end
 if frame>=980 and frame<996 then pad(2,"C",1)end
 if frame>=1020 and frame<1036 then pad(2,"Start",1)end
 if not installed and frame>=1100 and m:read_u16(LW)>0 and m:read_u8(GG+84)==2 then
  installed=true
  joy=m:install_read_tap(JOY,JOY+3,"fixed-simulation-input",function(o,d,mask)
   local input=8
   if index%16<3 then input=input|16 end
   if index%200<24 then input=input|1 elseif index%200<48 then input=input|2 end
   return input
  end)
  complete=m:install_write_tap(PC+2,PC+3,"completed-gameplay-state",function(o,d,mask)
   if terminal then return end
   f:write(tostring(index)..","..hex(GG,94)..","..hex(ENT,48*42)..","..hex(MOTION,28)..hex(ANIM,10)..hex(LIFE,36)..hex(OWN,2)..hex(PATH,2)..hex(WORLD,20)..hex(FOES,240)..hex(WEAPONS,72)..hex(IA,96)..hex(IW,10)..hex(WAVES,106)..hex(LARGE,102)..hex(KPOOL,64).."\\n")
   for k=0,1 do m:write_u8(GG+k*42+32,99)end
   index=index+1
   if index==FRAMES then terminal=true;f:close();manager.machine:exit()end
  end)
 end
 if frame>FRAMES*2+3000 then terminal=true;f:close();manager.machine:exit()end
end)
'''
 for key,value in {'OUTPUT':json.dumps(str(out/(name+'.csv'))),'GG':syms['g_game'],'ENT':syms['g_entities'],'JOY':syms['joyState'],'LW':syms['g_levelW'],'PC':syms['g_presentCounter'],'FRAMES':a.frames,**{k:syms[n]for k,n in {'MOTION':'player_motion','ANIM':'player_animation','LIFE':'player_life','OWN':'life_owned','PATH':'life_path','WORLD':'arcade_world','FOES':'arcade_foes','WEAPONS':'arcade_weapons','IA':'arcade_i_actors','IW':'arcade_i_wave','WAVES':'arcade_waves','LARGE':'arcade_large_actors','KPOOL':'arcade_k_actors'}.items()}}.items():s=re.sub(r'\b'+key+r'\b',str(value),s)
 # Include shared counters and RNG: equal visible actors alone can hide a
 # future divergence in spawning, conversion, or column timing.
 extra=[('arcade_rng',4),('wave_clock',1),('arcade_bonus_next',1),
        ('arcade_bonus_limit',1),('g_scrollSub',2),('g_scrollColumnLatch',1),
        ('g_scrollColumnAdvance',1),('arcade_shot_budget',1),
        ('formation_reservation',2),('player_shot_available',2),
        ('pool',202),('counter',2),('screen',2),('writes',20),('eaten',20),
        ('columns',16),('rows',8),('collected',1),('g_foodRound',1),
        ('arcade_food_spawn',4),('arcade_food_pickup',16),
        ('arcade_food_conversion_spawn',4),('arcade_food_conversion',16),
        ('arcade_food_conversion_pulse',1),('arcade_food_orb',1),
        ('g_arcadeSection',12),('g_sectionGrade',1),('section_variant',1),
        ('barrier_cursor',20),('barrier_writes',20),('barrier_pose',20),
        ('arcade_food_carrier',16)]
 # Two modules use a local 'dirty'. Select the food owner's flag by its
 # verified location between its row array and collection counter.
 dirties=[int(v,16)&0xffffff for v in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+dirty\s*$',symbols.read_text(),re.M)]
 food_dirty=[v for v in dirties if syms['rows']+8<=v<syms['collected']]
 assert len(food_dirty)==1
 suffix=''.join('..hex(%d,%d)'%(syms[n],size) for n,size in extra)+'..hex(%d,1)'%food_dirty[0]
 marker='hex(%d,64)'%syms['arcade_k_actors']
 assert s.count(marker)==1
 s=s.replace(marker,marker+suffix)
 script=out/(name+'.lua');script.write_text(s)
 subprocess.run(['mame','genesis','-cart',str(rom.resolve()),'-window','-nomaximize','-resolution','512x448','-video','none','-seconds_to_run','1000','-keyboardprovider','none','-mouseprovider','none','-joystickprovider','none','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_script',str(script),'-cfg_directory',str(out/name/'cfg'),'-nvram_directory',str(out/name/'nvram')],cwd=r,stdout=(out/(name+'.log')).open('w'),stderr=subprocess.STDOUT,check=True,timeout=180)
before=(out/'before.csv').read_text().splitlines();after=(out/'after.csv').read_text().splitlines();assert len(before)==len(after)==a.frames,(len(before),len(after))
for tick,(x,y)in enumerate(zip(before,after)):
 assert x==y,{'tick':tick,'before':x,'after':y}
result={'frames':a.frames,'state_bytes_per_frame':sum(len(field)//2 for field in before[0].split(',')[1:]),'mismatches':0,'baseline_rom_sha256':hashlib.sha256((a.baseline/'rom.bin').read_bytes()).hexdigest(),'candidate_rom_sha256':hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest(),'scope':'Two-player completed-update public player/entity and private motion/animation/life/world/foe/weapon/I/wave/large/K state; simulation-index controller reads, lives refilled'}
(out/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
