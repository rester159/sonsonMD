#!/usr/bin/env python3
"""Controller-only coin/Start during demo, both pads; no target state injection."""
from pathlib import Path
import subprocess,re,json,hashlib,os,csv
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/demo-start';o.mkdir(parents=True,exist_ok=True)
core=Path(os.environ.get('GENPLUS_CORE',str(Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib')))
prior=json.loads((r/'out/parity-audit/attract-runtime/report.json').read_text())
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==v for p,v in prior['hashes'].items())
syms={n:int(v,16) for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
s=(r/'out/parity-audit/attract-runtime/harness.c').read_text();a=s.index('static int16_t input(');b=s.index('#define FN',a)
s=s[:a]+'''static unsigned requested,coin;
static int16_t input(unsigned port,unsigned device,unsigned index,unsigned id){
 (void)device;(void)index;
 if(frame<740)return port==0&&id==3&&frame%24<12;
 return port==requested&&frame>=2000&&frame<2012&&(id==3||(coin&&id==8));
}
'''+s[b:]
s=s.replace('if(argc!=4)return 2;','if(argc!=6)return 2;requested=atoi(argv[4]);coin=atoi(argv[5]);')
s=s.replace('frame<10000','frame<2400')
s=s.replace('  retro_run();',f'''  if(frame==1990 && !r8(ram,{syms['g_attract']}))return 10;
  retro_run();
  if(frame==2300){{
   unsigned active=r8(ram,{syms['g_startPlayers']}),credits=r16(ram,{syms['g_arcadeCredits']}),attr=r8(ram,{syms['g_attract']});
   if(attr||credits)return 11;
   if(coin){{
    if(active!=(1u<<requested))return 12;
    if(r8(ram,{syms['g_game']}+requested*42+32)==0)return 13;
    if(r8(ram,{syms['g_game']}+(requested^1)*42+32)!=0)return 14;
   }}else if(r32(ram,{syms['g_frameCounter']})>600)return 15;
   printf("START_PASS player=%u coin=%u credits=%u\\n",requested+1,coin,credits);
  }}''')
(o/'harness.c').write_text(s);subprocess.run(['cc','-O2',str(o/'harness.c'),'-o',str(o/'harness')],check=True)
for p in (0,1):
 for coin in (0,1):
  with (o/f'log-{p}-{coin}.txt').open('w') as log:
   subprocess.run([str(o/'harness'),str(core),str(r/'out/rom.bin'),str(o/f'trace-{p}-{coin}.csv'),str(p),str(coin)],check=True,stdout=log,stderr=subprocess.STDOUT)
for player in (0,1):
 rows=list(csv.DictReader((o/f'trace-{player}-0.csv').open()))
 tail=[row for row in rows if int(row['refresh'])>=2100]
 assert len({row['tick'] for row in tail})==1 and {row['attract'] for row in tail}=={'0'},'Unpaid Start must remain idle at title'
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==v for p,v in prior['hashes'].items())
report={'scenarios':4,'hashes':prior['hashes'],'scope':'Controller-only boot to demo, then one Start edge with/without simultaneous coin from each pad. Checks demo exit, credit balance and only requested player lives. No injected RAM. Does not establish original arcade start lifecycle or all DIP combinations.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
