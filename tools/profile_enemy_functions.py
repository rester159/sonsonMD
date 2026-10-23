#!/usr/bin/env python3
"""Time unconditional entity-update boundaries; labels emit no instructions."""
from pathlib import Path
import csv,hashlib,json,re,subprocess,argparse
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--build-dir',type=Path,default=r/'out');p.add_argument('--output-dir',type=Path,default=r/'out/parity-audit/enemy-block-profile');args=p.parse_args()
build=args.build_dir.resolve();o=args.output_dir.resolve();o.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
identity=sha(build/'rom.bin');work=r/'out/parity-audit/workload-comparison';report=json.loads((work/'results.json').read_text())
name='after'if report['candidate_rom_sha256']==identity else'before';assert report[name=='after'and'candidate_rom_sha256'or'baseline_rom_sha256']==identity
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(build/'symbol.txt').read_text(),re.M)}
if any(n.startswith('profile_schedule_begin_') for n in syms):
    raise SystemExit('Duplicated compiler blocks require the windowless build_subsystem_profile.py probes; this historical single-address profiler does not support this build.')
dis=subprocess.check_output(['m68k-elf-objdump','-d',str(build/'rom.out')],text=True)
match=re.search(r'^\s*([0-9a-f]+):\s+((?:[0-9a-f]{4} )+)\s*jsr[^\n]*<entity_update_all>',dis,re.M);assert match
points=[syms['entity_update_all'],syms['profile_collision_begin'],syms['profile_schedule_begin'],int(match[1],16)+2*len(match[2].split())]
base=(work/(name+'.lua')).read_text().replace(str(work/(name+'.csv')),str(o/'times.csv'))
mark='local taps={};local times={}\nlocal function now()return manager.machine.time:as_double()end\nlocal function install_taps()\n'
for k,pc in enumerate(points,1):mark+=f'taps[#taps+1]=m:install_read_tap({pc},{pc+1},"enemy-block-{k}",function()times[{k}]=now()end)\n'
mark+='end\n'
base=base.replace('emu.register_frame_done(function()',mark+'emu.register_frame_done(function()',1).replace(' frame=frame+1',' frame=frame+1;if frame==60 then install_taps()end',1)
a=base.index('   f:write(tostring(index)');b=base.index('\n',a)
base=base[:a]+'''   assert(times[1]and times[2]and times[3]and times[4],'missing boundary')
   f:write(string.format("%d,%.9f,%.9f,%.9f\\n",index,times[2]-times[1],times[3]-times[2],times[4]-times[3]));times={}'''+base[b:]
(o/'run.lua').write_text(base)
with (o/'mame.log').open('w')as f:subprocess.run(['mame','genesis','-cart',str(build/'rom.bin'),'-window','-nomaximize','-resolution','512x448','-video','none','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_delay','0','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],stdout=f,stderr=subprocess.STDOUT,check=True,timeout=360)
assert sha(build/'rom.bin')==identity
rows=list(csv.reader((o/'times.csv').open()));assert len(rows)==report['frames'];assert all(float(x[k])>0 for x in rows for k in range(1,4))
result={'rom_sha256':identity,'samples':len(rows),'scope':'Elapsed emulated time at unconditional ROM-read boundaries, interrupts included; assembly labels emit zero instructions','sections':{fn:{'mean_ms':sum(float(x[k+1])for x in rows)/len(rows)*1000,'peak_ms':max(float(x[k+1])for x in rows)*1000}for k,fn in enumerate(['actor_updates','collisions','scheduler'])}}
(o/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
