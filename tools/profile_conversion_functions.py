#!/usr/bin/env python3
"""Measure actor calls using dynamic stack return addresses, at direct caller boundaries; tail calls are included in caller duration."""
from pathlib import Path
import re,json,hashlib,subprocess,csv,argparse
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--build-dir',type=Path,default=r/'out');p.add_argument('--output-dir',type=Path,default=r/'out/parity-audit/conversion-function-profile');p.add_argument('--frames',type=int);p.add_argument('--functions',help='Comma-separated direct-call symbols');args=p.parse_args()
build=args.build_dir.resolve();o=args.output_dir.resolve();o.mkdir(parents=True,exist_ok=True)
work=r/'out/parity-audit/workload-comparison';report=json.loads((work/'results.json').read_text())
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
(o/'results.json').unlink(missing_ok=True)
rh=sha(build/'rom.bin');name='after' if rh==report['candidate_rom_sha256'] else 'before'
assert rh==report['candidate_rom_sha256' if name=='after' else 'baseline_rom_sha256']
syms={n:int(a,16)&0xffffff for a,n in re.findall(r'^([0-9a-fA-F]+)\s+[tT]\s+(\w+)\s*$',(build/'symbol.txt').read_text(),re.M)}
dis=subprocess.check_output(['m68k-elf-objdump','-d',str(build/'rom.out')],text=True)
functions=['entity_update_all','entity_render_all','arcade_food_world_step_clock','arcade_food_world_contact','arcade_food_world_render','arcade_food_bonus_step','arcade_food_bonus_contact','arcade_food_bonus_background','arcade_food_bonus_sat','entity_arcade_conversion_contact','game_arcade_floor','arcade_large_present','arcade_food_world_shot','arcade_food_world_player','arcade_barrier_step','arcade_barrier_upload']
if args.functions:functions=args.functions.split(',')
mark='local taps={};local starts={};local elapsed={};local calls={}\nlocal function now()return manager.machine.time:as_double()end\nlocal function install_taps()\n'
for i,fn in enumerate(functions,1):
 sites=[]
 for match in re.finditer(r'^\s*([0-9a-f]+):\s+((?:[0-9a-f]{4} )+)\s*(?:jsr|bsr\w*)[^\n]*<'+fn+r'>',dis,re.M):
  pc=int(match[1],16);sites.append((pc,pc+2*len(match[2].split())))
 for j,(pc,ret) in enumerate(sites):
  mark+=f'taps[#taps+1]=m:install_read_tap({pc},{pc+1},"begin-{i}-{j}",function()starts[{i}]=now()end)\n'
  mark+=f'taps[#taps+1]=m:install_read_tap({ret},{ret+1},"end-{i}-{j}",function()if starts[{i}]then elapsed[{i}]=(elapsed[{i}]or 0)+now()-starts[{i}];calls[{i}]=(calls[{i}]or 0)+1;starts[{i}]=nil end end)\n'
mark+='end\n'
base=(work/(name+'.lua')).read_text().replace(str(work/(name+'.csv')),str(o/'times.csv'))
frames=args.frames or report['frames'];assert 1<frames<=report['frames']
base=base.replace('index=='+str(report['frames']), 'index=='+str(frames))
base=base.replace('emu.register_frame_done(function()',mark+'emu.register_frame_done(function()',1).replace(' frame=frame+1',' frame=frame+1;if frame==60 then install_taps()end',1)
a=base.index('   f:write(tostring(index)');b=base.index('\n',a)
base=base[:a]+'''   assert(next(starts)==nil,'unclosed function call');f:write(tostring(index));for k=1,FUNCTION_COUNT do f:write(string.format(",%.9f,%d",elapsed[k]or 0,calls[k]or 0))end;f:write("\\n");elapsed={};calls={}'''+base[b:]
base=base.replace('FUNCTION_COUNT',str(len(functions)))
(o/'run.lua').write_text(base)
with (o/'mame.log').open('w') as f:
 subprocess.run(['mame','genesis','-cart',str(build/'rom.bin'),'-window','-nomaximize','-resolution','512x448','-video','none','-keyboardprovider','none','-mouseprovider','none','-joystickprovider','none','-seconds_to_run','1000','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_delay','0','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],stdout=f,stderr=subprocess.STDOUT,check=True,timeout=360)
assert sha(build/'rom.bin')==rh
rows=list(csv.reader((o/'times.csv').open()));assert len(rows)==frames
# Select actual source-cell conversion calls, rather than inactive large slots.
busy=rows[1:] # First capture includes warmup calls before the fixed workload starts.
assert busy
result={'rom_sha256':rh,'source_workload_updates':report['frames'],'requested_prefix_updates':frames,'updates':len(rows),'profiled_updates':len(busy),'discarded_warmup_updates':1,'scope':'Direct caller-boundary elapsed time includes callees and interrupts; indirect calls excluded; do not sum inclusive durations','functions':{fn:{'mean_ms':sum(float(row[1+2*i]) for row in busy)*1000/len(busy),'peak_ms':max(float(row[1+2*i]) for row in busy)*1000,'calls':sum(int(row[2+2*i]) for row in busy),'observed_direct_calls':any(int(row[2+2*i]) for row in busy)} for i,fn in enumerate(functions)}}
(o/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
