#!/usr/bin/env python3
"""Freeze DIP target fixture; explicit DIP injection during a natural demo."""
from pathlib import Path
import re,json,hashlib,subprocess,csv,os
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/freeze-runtime';o.mkdir(parents=True,exist_ok=True)
prior=json.loads((r/'out/parity-audit/attract-runtime/report.json').read_text())
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in prior['hashes'].items())
syms={n:int(v,16) for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
s=(r/'out/parity-audit/attract-runtime/harness.c').read_text()
s=s.replace('  retro_run();',f'''  if(frame==2200)w8(ram,{syms['g_config']+1},r8(ram,{syms['g_config']+1})&127);
  if(frame==2600)w8(ram,{syms['g_config']+1},r8(ram,{syms['g_config']+1})|128);
  retro_run();''')
s=s.replace('delay0,delay1\\n','delay0,delay1,p1x,p1y,p2x,p2y,camera\\n')
s=s.replace("  fputc('\\n',f);",f'''  fprintf(f,",%u,%u,%u,%u,%u",r16(ram,{syms['g_game']}),r16(ram,{syms['g_game']+2}),r16(ram,{syms['g_game']+42}),r16(ram,{syms['g_game']+44}),r16(ram,{syms['g_game']+88}));
  fputc('\\n',f);''')
(o/'harness.c').write_text(s);subprocess.run(['cc','-O2',str(o/'harness.c'),'-o',str(o/'harness')],check=True)
core=Path(os.environ.get('GENPLUS_CORE',str(Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib')))
with (o/'log.txt').open('w') as log:subprocess.run([str(o/'harness'),str(core),str(r/'out/rom.bin'),str(o/'trace.csv')],check=True,stdout=log,stderr=subprocess.STDOUT)
rows=[{k:int(v) for k,v in x.items()} for x in csv.DictReader((o/'trace.csv').open())]
frozen=[x for x in rows if 2220<=x['refresh']<2600]
assert len(frozen)==380 and {x['attract'] for x in frozen}=={1}
fields=['cursor0','cursor1','input0','input1','previous0','previous1','delay0','delay1','p1x','p1y','p2x','p2y','camera']
for field in fields:assert len({x[field] for x in frozen})==1,field
assert frozen[-1]['tick']-frozen[0]['tick']>300
assert len({x['clock'] for x in frozen})==256
assert rows[2700]['cursor0']!=frozen[0]['cursor0']
exits=[x['refresh'] for a,x in zip(rows,rows[1:]) if a['attract'] and not x['attract']]
assert exits and exits[0]>4000,exits
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in prior['hashes'].items())
report={'hashes':prior['hashes'],'frozen_refreshes_checked':len(frozen),'frozen_fields':fields,'clock_values_seen':256,'demo_exit_refreshes':exits,'scope':'Target Freeze DIP injected for400 refreshes during demo. Clock continues, player/camera/input state stays fixed, resumes after clearing, demo expiry delayed. No visual/audio or menu DIP claim.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
