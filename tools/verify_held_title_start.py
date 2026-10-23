#!/usr/bin/env python3
"""Source title samples held Start; runtime cases for delayed coin and Freeze release."""
from pathlib import Path
import subprocess,re,json,hashlib,csv
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/held-title-start';o.mkdir(parents=True,exist_ok=True)
rom=b''.join((r/'arcade'/n).read_bytes() for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(rom).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
assert rom[0x92b8-0x4000:0x92c0-0x4000]==bytes.fromhex('d63cc40f272d8d2c')
prior=json.loads((r/'out/parity-audit/attract-runtime/report.json').read_text());assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in prior['hashes'].items())
syms={n:int(v,16) for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
s=(r/'out/parity-audit/attract-runtime/harness.c').read_text();a=s.index('static int16_t input(');b=s.index('#define FN',a)
s=s[:a]+'''static unsigned scenario,player;
static int16_t input(unsigned port,unsigned device,unsigned index,unsigned id){
 (void)device;(void)index;
 if(frame<740)return port==0&&id==3&&frame%24<12;
 if(port!=player)return 0;
 if(scenario==2)return id==3&&frame>=900&&frame<2050;
 if(scenario==0)return (id==3&&frame>=900&&frame<1500)||(id==8&&frame>=1000&&frame<1012);
 return (id==8&&frame>=950&&frame<962)||(id==3&&frame>=1000&&frame<1500);
}
'''+s[b:]
s=s.replace('if(argc!=4)return 2;','if(argc!=6)return 2;scenario=atoi(argv[4]);player=atoi(argv[5]);').replace('frame<10000','frame<2100')
s=s.replace('  retro_run();',f'''  if(scenario==1 && frame==900)w8(ram,{syms['g_config']+1},r8(ram,{syms['g_config']+1})&127);
  if(scenario==1 && frame==1200)w8(ram,{syms['g_config']+1},r8(ram,{syms['g_config']+1})|128);
  retro_run();''')
s=s.replace('delay0,delay1\\n','delay0,delay1,credits,started,life0,life1\\n').replace("  fputc('\\n',f);",f"  fprintf(f,\",%u,%u,%u,%u\",r16(ram,{syms['g_arcadeCredits']}),r8(ram,{syms['g_startPlayers']}),r8(ram,{syms['g_game']+32}),r8(ram,{syms['g_game']+74}));\n  fputc('\\n',f);")
(o/'harness.c').write_text(s);subprocess.run(['cc','-O2',str(o/'harness.c'),'-o',str(o/'harness')],check=True)
core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib';checks=[]
for scenario in (0,1,2):
 for player in (0,1):
  with (o/f'log-{scenario}-{player}.txt').open('w') as log:subprocess.run([str(o/'harness'),str(core),str(r/'out/rom.bin'),str(o/f'trace-{scenario}-{player}.csv'),str(scenario),str(player)],check=True,stdout=log,stderr=subprocess.STDOUT)
  rows=[{k:int(v) for k,v in row.items()} for row in csv.DictReader((o/f'trace-{scenario}-{player}.csv').open())]
  if scenario==2:
   checks.append({'scenario':scenario,'player':player,'unpaid_demo_runs':rows[2000]['attract']==1,'credit_stays_zero':rows[2000]['credits']==0,'meaning':'Unpaid held Start does not block attract demo','trace_sha256':hashlib.sha256((o/f'trace-{scenario}-{player}.csv').read_bytes()).hexdigest()})
   continue
  end=rows[1400];wait=rows[1100:1190]
  checks.append({'scenario':scenario,'player':player,'started_requested_player':end['started']==1<<player and end[f'life{player}']>0 and end[f'life{player^1}']==0,'spent_one_credit':end['credits']==0,'frozen_before_release':scenario==0 or (all(x['credits']==1 and x['attract']==0 for x in wait) and len({x['tick'] for x in wait})==1),'trace_sha256':hashlib.sha256((o/f'trace-{scenario}-{player}.csv').read_bytes()).hexdigest()})
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in prior['hashes'].items())
for check in checks:
 keys=('unpaid_demo_runs','credit_stays_zero') if check['scenario']==2 else ('started_requested_player','spent_one_credit','frozen_before_release')
 check['passed']=all(check[k] for k in keys)
report={'checks':checks,'hashes':prior['hashes'],'scope':'Controller-only late coin while Start held, or injected Freeze with Start held across release; both controller mappings; unpaid held Start still allows attract demo. Source held-level semantics plus requested per-controller extension. Not original 2P-start selection policy, debounce or boot clock parity.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));assert all(x['passed'] for x in checks)
