#!/usr/bin/env python3
"""Windowless game-over return fixture; final-death state is injected."""
from pathlib import Path
import argparse,csv,hashlib,json,os,re,subprocess
p=argparse.ArgumentParser();p.add_argument('--expect-failure',action='store_true');args=p.parse_args()
r=Path(__file__).resolve().parents[1]
program=b''.join((r/'arcade'/n).read_bytes() for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(program).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
assert program[0x57ed:0x57f9].hex()=='5fd702d705d708d70bd70e39'
o=r/'out/parity-audit/arcade-return'/('before' if args.expect_failure else 'after');o.mkdir(parents=True,exist_ok=True)
prior=json.loads((r/'out/parity-audit/attract-runtime/report.json').read_text())
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in prior['hashes'].items())
syms={n:int(v,16) for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
s=(r/'out/parity-audit/attract-runtime/harness.c').read_text();a=s.index('static int16_t input(');b=s.index('#define FN',a)
s=s[:a]+'''static int16_t input(unsigned port,unsigned device,unsigned index,unsigned id){
 (void)device;(void)index;if(port)return 0;
 if(frame<740)return id==3 && frame%24<12;
 if(frame>=1000&&frame<1012)return id==3||id==8;
 if(frame>=1060&&frame<1072)return id==8;
 if(frame>=2200&&frame<2212)return id==3;
 return 0;
}
'''+s[b:]
s=s.replace('frame<10000','frame<2600')
s=s.replace('  retro_run();',f'''  if(frame==2000){{
    w8(ram,{syms['g_game']+34},3);w8(ram,{syms['g_game']+76},3);
    w8(ram,{syms['g_game']+32},0);w8(ram,{syms['g_game']+74},0);
    w8(ram,{syms['g_gameOverBeat']},0);w8(ram,{syms['g_gameOverBeat']+1},1);
  }}
  retro_run();''')
s=s.replace('delay0,delay1\\n','delay0,delay1,credits,lives,mode\\n')
s=s.replace("  fputc('\\n',f);",f'''  fprintf(f,",%u,%u,%u",r16(ram,{syms['g_arcadeCredits']}),r8(ram,{syms['g_game']+32}),r8(ram,{syms['g_mode']}));
  fputc('\\n',f);''')
(o/'harness.c').write_text(s)
subprocess.run(['cc','-O2',str(o/'harness.c'),'-o',str(o/'harness')],check=True)
core=Path(os.environ.get('GENPLUS_CORE',str(Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib')))
with (o/'log.txt').open('w') as log:
 subprocess.run([str(o/'harness'),str(core),str(r/'out/rom.bin'),str(o/'trace.csv')],check=True,stdout=log,stderr=subprocess.STDOUT)
rows=[{k:int(v) for k,v in row.items()} for row in csv.DictReader((o/'trace.csv').open())]
assert rows[1200]['attract']==0 and rows[1200]['credits']==1 and rows[1200]['lives']>0
assert rows[2100]['lives']==0 and rows[2100]['credits']==1
passed=rows[2400]['credits']==0 and rows[2400]['lives']>0 and rows[2400]['attract']==0
report={'passed':passed,'at_2100':rows[2100],'at_2400':rows[2400],
 'hashes':prior['hashes'],'scope':'Controller start/extra coin, injected both-player game-over with one remaining beat, then controller Start after return. Checks retained credit and playable restart without selector/DIP interaction. Not natural final-death or ranking/name-entry lifecycle validation.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
assert passed!=args.expect_failure
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in prior['hashes'].items())
