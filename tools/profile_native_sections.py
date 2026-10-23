#!/usr/bin/env python3
"""Measure native section elapsed emulated time with ROM read taps; no ROM edits.
Uses the fixed-input completed-update fixture from compare_native_workloads.py.
"""
from pathlib import Path
import csv,hashlib,json,re,subprocess,argparse
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--build-dir',type=Path,default=r/'out');p.add_argument('--output-dir',type=Path,default=r/'out/parity-audit/section-profile');args=p.parse_args()
build=args.build_dir.resolve();o=args.output_dir.resolve();o.mkdir(parents=True,exist_ok=True)
identity=hashlib.sha256((build/'rom.bin').read_bytes()).hexdigest()
work=r/'out/parity-audit/workload-comparison';report=json.loads((work/'results.json').read_text())
name='after' if report['candidate_rom_sha256']==identity else 'before'
assert report['candidate_rom_sha256' if name=='after' else 'baseline_rom_sha256']==identity
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(build/'symbol.txt').read_text(),re.M)}
dis=subprocess.check_output(['m68k-elf-objdump','-d',str(build/'rom.out')],text=True)
def return_site(fn):
 match=re.search(r'^\s*([0-9a-f]+):\s+((?:[0-9a-f]{4} )+)\s*jsr[^\n]*<'+fn+r'>',dis,re.M);assert match,fn
 return int(match[1],16)+2*len(match[2].split())
start=syms['snd_vblank'];later=min(v for v in syms.values()if v>start)
returns=[int(v,16)for v in re.findall(r'^\s*([0-9a-f]+):\s+4e75\s+rts',dis,re.M)if start<=int(v,16)<later]
assert returns
points=[syms['game_update'],syms['entity_update_all'],return_site('entity_update_all'),syms['game_render'],syms['entity_render_all'],return_site('entity_render_all')]
base=(work/(name+'.lua')).read_text();base=base.replace(str(work/(name+'.csv')),str(o/'times.csv'))
mark='local times={};local taps={};local audio_start=0;local audio_elapsed=0\n'
mark+='local function now()return manager.machine.time:as_double()end\nlocal function install_taps()\n'
for k,pc in enumerate(points,1):mark+=f'taps[#taps+1]=m:install_read_tap({pc},{pc+1},"section-{k}",function()times[{k}]=now()end)\n'
mark+=f'taps[#taps+1]=m:install_read_tap({start},{start+1},"audio-start",function()audio_start=now()end)\n'
for pc in returns:mark+=f'taps[#taps+1]=m:install_read_tap({pc},{pc+1},"audio-end-{pc}",function()audio_elapsed=audio_elapsed+now()-audio_start end)\n'
mark+='end\n'
base=base.replace('emu.register_frame_done(function()',mark+'emu.register_frame_done(function()',1)
base=base.replace(' frame=frame+1',' frame=frame+1;if frame==60 then install_taps()end',1)
a=base.index('   f:write(tostring(index)');b=base.index('\n',a)
base=base[:a]+'''   local t=now()
   f:write(string.format("%d,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\\n",index,(times[2]or t)-(times[1]or t),(times[3]or t)-(times[2]or t),(times[4]or t)-(times[3]or t),(times[5]or t)-(times[4]or t),(times[6]or t)-(times[5]or t),t-(times[6]or t)))
   audio_elapsed=0'''+base[b:]
(o/'run.lua').write_text(base)
subprocess.run(['mame','genesis','-cart',str(build/'rom.bin'),'-window','-nomaximize','-resolution','512x448','-video','none','-keyboardprovider','none','-mouseprovider','none','-joystickprovider','none','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],cwd=r,stdout=(o/'mame.log').open('w'),stderr=subprocess.STDOUT,check=True,timeout=360)
rows=list(csv.reader((o/'times.csv').open()));assert len(rows)==json.loads((work/'results.json').read_text())['frames']
labels=['players','entities_collision_scheduler','flow_bookkeeping','background_stream','entity_sat','player_sat_hud']
result={'rom_sha256':identity,'samples':len(rows),'measurement':'Elapsed emulated seconds between ROM read taps at function/call boundaries; includes interrupts if they occur in a section','sections':{name:{'mean_ms':sum(float(v[k+1])for v in rows)/len(rows)*1000,'peak_ms':max(float(v[k+1])for v in rows)*1000}for k,name in enumerate(labels)}}
assert all(v['peak_ms']>0 for v in result['sections'].values()),'Missing read-tap timing events'
(o/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
