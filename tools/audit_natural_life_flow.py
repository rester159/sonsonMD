#!/usr/bin/env python3
"""Windowless neutral-input life flow, without RAM writes or life refills."""
from pathlib import Path
import csv,hashlib,json,os,re,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/natural-life-flow';o.mkdir(parents=True,exist_ok=True)
syms={n:v for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)(?:\.lto_priv\.\d+)?\s*$',(r/'out/symbol.txt').read_text(),re.M)}
s=(r/'tools/verify_libretro.c').read_text()
a=s.index('        if(!started)w8(ram,np,players);');b=s.index('        retro_run();',a)
s=s[:a]+s[b:]
a=s.index('    input_step=simulation_input?');b=s.index('\n}',a)
s=s[:a]+'    return 0; /* Neutral controller after actual coin/start sequence. */'+s[b:]
s=s.replace('        retro_run();','        retro_run();\n        if(started && r8(ram,attr))break; /* Next attract cycle is a different session. */')
s=s.replace('return (fixed_ticks?r32(ram,fc)-start_tick>=limit:sample==limit)?0:1;', 'return started?0:1;')
mark='        else {\n            unsigned entities='
assert s.count(mark)==1
probe='''        else {
            static unsigned last_tick;unsigned now=r32(ram,fc);
            if(now!=last_tick){
                for(unsigned k=0;k<players;k++){
                    unsigned p=gg+k*42,l=LIFE+k*18;
                    printf("LIFE %u %u %u %u %u %u %u %u %u %u %u %u\\n",now,k,r8(ram,p+34),r8(ram,p+32),r8(ram,OWN+k),r8(ram,l+6),r8(ram,l+7),r8(ram,l+8),r8(ram,l+5),r16(ram,gg+88),r16(ram,BEAT),now-last_tick);
                }last_tick=now;
            }
            unsigned entities='''
for key,name in [('LIFE','player_life'),('OWN','life_owned'),('BEAT','g_gameOverBeat')]:
 # Do not replace the LIFE output marker.
 probe=probe.replace(key+'+', '0x'+syms[name]+'+').replace('ram,'+key+')','ram,0x'+syms[name]+')')
s=s.replace(mark,probe);(o/'run.c').write_text(s)
subprocess.run(['cc','-O2',str(o/'run.c'),'-o',str(o/'run')],check=True)
core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib';rh=hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest();runs=[]
(o/'report.json').unlink(missing_ok=True)
for players in (1,2):
 log=o/f'{players}p.log'
 with log.open('w')as f:
  subprocess.run([str(o/'run'),str(core),str(r/'out/rom.bin'),str(players),'12000',*[syms[n]for n in ('g_frameCounter','g_presentCounter','g_levelW','g_game','g_numPlayers','g_attract')],str(o/f'{players}p.csv')],stdout=f,stderr=subprocess.STDOUT,check=True)
 rows=[list(map(int,line.split()[1:]))for line in log.read_text().splitlines()if line.startswith('LIFE ')]
 states=[]
 for owner in range(players):
  owned=[x for x in rows if x[1]==owner];events=[];previous=None
  for x in owned:
   key=tuple(x[2:8])
   if key!=previous:events.append(x);previous=key
  assert {x[2]for x in owned}=={0,1,2,3},('missing flow',owner)
  changes=[(a,b)for a,b in zip(owned,owned[1:])if a[3]!=b[3]]
  assert [(a[3],b[3])for a,b in changes]==[(3,2),(2,1),(1,0)],('life changes',owner,changes)
  for before,after in changes:
   assert after[11]==1,('unsampled decrement',owner,after)
   if after[3]:assert after[4]==1 and after[5]==6,('reserve consumed outside source state5',owner,after)
   else:assert after[2]==3 and after[4]==2 and after[5]==4,('final source gate',owner,after)
  states.append({'owner':owner,'flows':sorted({x[2]for x in owned}),'life_counts':sorted({x[3]for x in owned}),'source_life_states':sorted({x[5]for x in owned if x[4]==1}),'transitions':events})
 runs.append({'players':players,'observations':len(rows),'owners':states})
 print(players,[(x['flows'],x['life_counts'],x['source_life_states'])for x in states],flush=True)
assert hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest()==rh
(o/'report.json').write_text(json.dumps({'rom_sha256':rh,'core_sha256':hashlib.sha256(core.read_bytes()).hexdigest(),'passed':True,'scope':'Controller coin/start then neutral input,Up to12000refresh observations per mode, ending when the next attract cycle begins. No RAM injection/refills. Observes connected life flow; no assertion of full source timing/parity, continue behavior or successful human playthrough.','columns':['tick','owner','flow','lives','life_owned','state','phase','kind','active','camera','game_over_beat','tick_delta'],'runs':runs},indent=2)+'\n')
