#!/usr/bin/env python3
"""Target audio fixture: inject demo DIP only; measure core PCM during demos."""
from pathlib import Path
import re,subprocess,hashlib,json,os
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/demo-sound/runtime';o.mkdir(parents=True,exist_ok=True)
core=Path(os.environ.get('GENPLUS_CORE',str(Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib')))
previous=json.loads((r/'out/parity-audit/attract-runtime/report.json').read_text())
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in previous['hashes'].items()),'Run verify_attract_runtime.py on this build first'
symbols={n:int(v,16) for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
source=(r/'out/parity-audit/attract-runtime/harness.c').read_text()
a=source.index('static void audio(');b=source.index('static void poll',a)
source=source[:a]+'''static bool capture;
static unsigned long long samples,nonzero;
static unsigned peak;
static void audio(int16_t l,int16_t r) {
 if(!capture)return;
 int values[2]={l,r};
 for(unsigned k=0;k<2;k++){unsigned mag=values[k]<0?-values[k]:values[k];samples++;nonzero+=(mag!=0);if(mag>peak)peak=mag;}
}
static size_t batch(const int16_t*d,size_t n){for(size_t i=0;i<n;i++)audio(d[2*i],d[2*i+1]);return n;}
'''+source[b:]
source=source.replace('if(argc!=4)return 2;', 'if(argc!=5)return 2; unsigned enabled=atoi(argv[4]),demo_age=0;')
source=source.replace('  retro_run();',f'''  /* Explicit fixture injection, not proof of menu/save behavior. */
  if(frame>=1000){{
   unsigned dip=r8(ram,{symbols['g_config']});
   w8(ram,{symbols['g_config']},enabled?(dip&~32u):(dip|32u));
   w8(ram,{symbols['g_config']+5},enabled);
  }}
  if(r8(ram,{symbols['g_attract']}))demo_age++;else demo_age=0;
  capture=demo_age>120;
  retro_run();''')
source=source.replace('fclose(f);retro_unload_game();','printf("PCM %llu %llu %u\\n",samples,nonzero,peak);fclose(f);retro_unload_game();')
(o/'harness.c').write_text(source)
subprocess.run(['cc','-O2',str(o/'harness.c'),'-o',str(o/'harness')],check=True)
results=[]
for enabled in (0,1):
 with (o/f'log-{enabled}.txt').open('w') as log:
  subprocess.run([str(o/'harness'),str(core),str(r/'out/rom.bin'),str(o/f'trace-{enabled}.csv'),str(enabled)],check=True,stdout=log,stderr=subprocess.STDOUT)
 match=re.search(r'PCM (\d+) (\d+) (\d+)',(o/f'log-{enabled}.txt').read_text());assert match
 samples,nonzero,peak=map(int,match.groups());assert samples>1000000
 if enabled:assert nonzero>10000 and peak>100
 else:assert nonzero==0 and peak==0,(nonzero,peak)
 results.append({'demo_sound':enabled,'samples':samples,'nonzero':nonzero,'peak':peak})
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in previous['hashes'].items())
report={'results':results,'hashes':previous['hashes'],'scope':'Actual target core PCM after 120 demo refreshes settling. Raw/decoded demo DIP injected before demo startup; controller boot otherwise. Proves mute/output gate, not DIP UI/SRAM or audible arcade fidelity.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
