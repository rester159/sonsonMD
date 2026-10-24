#!/usr/bin/env python3
"""Controller-only requested join extension, both player orders."""
from pathlib import Path
import subprocess,re,json,hashlib,os,csv
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/join-runtime';o.mkdir(parents=True,exist_ok=True)
prior=json.loads((r/'out/parity-audit/attract-runtime/report.json').read_text())
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in prior['hashes'].items())
syms={n:int(v,16) for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
s=(r/'out/parity-audit/attract-runtime/harness.c').read_text();a=s.index('static int16_t input(');b=s.index('#define FN',a)
s=s[:a]+'''static unsigned first;
static int16_t input(unsigned port,unsigned device,unsigned index,unsigned id){
 (void)device;(void)index;
 if(frame<740)return port==0&&id==3&&frame%24<12;
 if(frame>=2000&&frame<2012)return port==first&&(id==3||id==8);
 if(frame>=2200&&frame<2212)return port==(first^1)&&(id==3||id==8);
 /* Give another credit, then hold Start on both active controllers. */
 if(frame>=2300&&frame<2312)return port==0&&id==8;
 if(frame>=2340&&frame<2400)return id==3;
 /* Check each entrant promptly, before ordinary combat can lock input. */
 if((port==first&&frame>=2030&&frame<2060)||(port==(first^1)&&frame>=2230&&frame<2260))return id==6;
 if((port==first&&frame>=2070&&frame<2100)||(port==(first^1)&&frame>=2270&&frame<2300))return id==7;
 return 0;
}
'''+s[b:]
s=s.replace('if(argc!=4)return 2;', 'if(argc!=5)return 2;first=atoi(argv[4]);').replace('frame<10000','frame<2560')
s=s.replace('delay0,delay1\\n','delay0,delay1,credits,paused,started,lives0,lives1,face0,face1,joy0,joy1,previous_motion0,attached0,x0,camera,flow0,flow1\\n')
s=s.replace("  fputc('\\n',f);",f'''  fprintf(f,",%u,%u,%u,%u,%u,%u,%u",r16(ram,{syms['g_arcadeCredits']}),r8(ram,{syms['g_paused']}),r8(ram,{syms['g_startPlayers']}),r8(ram,{syms['g_game']+32}),r8(ram,{syms['g_game']+74}),r8(ram,{syms['g_game']+15}),r8(ram,{syms['g_game']+57}));
  fputc('\\n',f);''')
s=s.replace("  fputc('\\n',f);",f"  fprintf(f,\",%u,%u,%u,%u,%u,%u,%u,%u\",r16(ram,{syms['joyState']}),r16(ram,{syms['joyState']+2}),r8(ram,{syms['player_motion']+10}),r8(ram,{syms['player_motion']+12}),r16(ram,{syms['g_game']}),r16(ram,{syms['g_game']+88}),r8(ram,{syms['g_game']+34}),r8(ram,{syms['g_game']+76}));\n  fputc('\\n',f);")
(o/'harness.c').write_text(s);subprocess.run(['cc','-O2',str(o/'harness.c'),'-o',str(o/'harness')],check=True)
core=Path(os.environ.get('GENPLUS_CORE',str(Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib')))
for first in (0,1):
 with (o/f'log-{first}.txt').open('w') as log:subprocess.run([str(o/'harness'),str(core),str(r/'out/rom.bin'),str(o/f'trace-{first}.csv'),str(first)],check=True,stdout=log,stderr=subprocess.STDOUT)
 rows=[{k:int(v) for k,v in x.items()} for x in csv.DictReader((o/f'trace-{first}.csv').open())]
 assert rows[1990]['attract']==1
 assert rows[2100]['credits']==0 and rows[2100][f'lives{first}']>0 and rows[2100][f'lives{first^1}']==0
 assert rows[2260]['players']==2 and rows[2260]['lives0']>0 and rows[2260]['lives1']>0 and rows[2260]['credits']==0
 held=rows[2340:2400]
 assert {x['credits'] for x in held}=={1} and {x['paused'] for x in held}=={0}
 assert held[-1]['tick']>held[0]['tick']
 # The initial player may already be dying: directional input is intentionally
 # locked then. Test the newly joined player in each order, covering both pads.
 # Require settled snapshots and an explicitly controllable flow state.
 for lo,hi,facing in ((2240,2260,1),(2280,2300,0)):
  samples=[x for x in rows[lo:hi] if any(a<=x['pc']<b for a,b in prior['coherent_wait_ranges'])]
  assert samples, (first,lo,'no coherent samples')
  assert all(x[f'flow{first^1}'] in (0,2) for x in samples),samples
  assert all(x[f'face{first^1}']==facing for x in samples),samples

assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in prior['hashes'].items())
report={'harness_sha256':hashlib.sha256((o/'harness.c').read_bytes()).hexdigest(),'trace_sha256':{str(i):hashlib.sha256((o/f'trace-{i}.csv').read_bytes()).hexdigest() for i in (0,1)},'orders':['P1 then P2','P2 then P1'],'hashes':prior['hashes'],'scope':'Controller-only cold boot through attract to paid game, paid other-player join, held Start without pause/extra spending, new entrant left/right responses in both controller orders. Initial-player movement during death is outside this assertion. No RAM writes, life refill or CPU redirection. Validates requested console extension, not original arcade join semantics or full continue lifecycle.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
