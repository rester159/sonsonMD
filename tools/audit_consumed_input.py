#!/usr/bin/env python3
"""Isolated target capture: simulation-indexed frontend input is not per-tick replay."""
from pathlib import Path
import shutil,subprocess,re,json,hashlib,csv,os,argparse
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("--tick-replay",action="store_true");args=parser.parse_args()
r=Path(__file__).resolve().parents[1];o=r/('out/parity-audit/consumed-input-replay' if args.tick_replay else 'out/parity-audit/consumed-input');o.mkdir(parents=True,exist_ok=True)
core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
reports=[];traces=[]
for variant in ('baseline','candidate'):
 d=o/variant;d.mkdir(exist_ok=True)
 for name in ('src','inc'):shutil.copytree(r/name,d/name,dirs_exist_ok=True)
 (d/'tools').mkdir(exist_ok=True);shutil.copy2(r/'tools/build_native.py',d/'tools/build_native.py')
 # Keep both historical sides explicit after production adopts the candidate.
 shutil.copy2(r/f'out/parity-audit/actor-empty-tail/{variant}/entity.c',d/'src/entity.c')
 p=d/'src/game.c';s=p.read_text();s=s.replace('bool game_update(void)\n{','volatile u32 diagnostic_input_count;\nvolatile u32 diagnostic_input_rows[64][3];\nbool game_update(void)\n{',1)
 needle='    g_frameCounter++;          /* headless FPS probe tick (see g_frameCounter) */'
 assert s.count(needle)==1
 s=s.replace(needle,needle+'''
    if(!g_attract && gameplay_input_ready){
        u16 row=(u16)(diagnostic_input_count&63);
        diagnostic_input_rows[row][0]=g_frameCounter;
        diagnostic_input_rows[row][1]=((u32)gameplay_pad[0]<<16)|gameplay_pad[1];
        diagnostic_input_rows[row][2]=entity_arcade_clock();
        diagnostic_input_count++;
    }
''')
 if args.tick_replay:
  s=s.replace('        u16 row=(u16)(diagnostic_input_count&63);', '''        u32 step=g_frameCounter-1;
        u16 pad=BUTTON_RIGHT | (step%16<3?BUTTON_A:0) | (step%200<24?BUTTON_UP:0) | (step%200>=24 && step%200<48?BUTTON_DOWN:0);
        gameplay_pad[0]=gameplay_pad[1]=pad;
        u16 row=(u16)(diagnostic_input_count&63);''')
 p.write_text(s)
 subprocess.run(['python3',str(d/'tools/build_native.py')],check=True)
 syms={n:v for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(d/'out/symbol.txt').read_text(),re.M)}
 s=(r/'tools/verify_libretro.c').read_text()
 s=s.replace('    for(frame=0;',f'''    unsigned seen=0;FILE *inputs=fopen("{d}/inputs.csv","w");if(!inputs)return 3;
    fputs("tick,pad0,pad1,clock,refresh\\n",inputs);
    for(frame=0;''',1)
 s=s.replace('        retro_run();',f'''        retro_run();
        unsigned count=r32(ram,0x{syms['diagnostic_input_count']});
        if(count-seen>64)return 4;
        while(seen<count){{
            unsigned address=0x{syms['diagnostic_input_rows']}+(seen&63)*12;
            unsigned pad=r32(ram,address+4);
            fprintf(inputs,"%u,%u,%u,%u,%u\\n",r32(ram,address),pad>>16,pad&65535,r32(ram,address+8),frame);seen++;
        }}''',1)
 s=s.replace('    fclose(f);printf(', '    fclose(inputs);fclose(f);printf(',1)
 (d/'harness.c').write_text(s);subprocess.run(['cc','-O2',str(d/'harness.c'),'-o',str(d/'harness')],check=True)
 env=os.environ.copy();env.update(SONSON_SIM_INPUT='1',SONSON_FIXED_TICKS='1')
 cmd=[str(d/'harness'),str(core),str(d/'out/rom.bin'),'2','3000',*[syms[n] for n in ('g_frameCounter','g_presentCounter','g_levelW','g_game','g_numPlayers','g_attract')],str(d/'refresh.csv')]
 with (d/'runtime.log').open('w') as log:subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
 rows=list(csv.DictReader((d/'inputs.csv').open()));assert len(rows)>3000
 ticks=[int(x['tick']) for x in rows];assert all(b==a+1 for a,b in zip(ticks,ticks[1:])), 'Missing simulation records'
 traces.append({int(x['tick']):(int(x['pad0']),int(x['pad1']),int(x['clock'])) for x in rows})
 reports.append({'variant':variant,'records':len(rows),'hashes':{str(p.relative_to(d)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (d/'out/rom.bin',d/'out/symbol.txt',d/'harness.c',d/'inputs.csv')}})
common=sorted(traces[0].keys()&traces[1].keys());different=[t for t in common if traces[0][t][:2]!=traces[1][t][:2]]
if args.tick_replay:assert not different, different[:12]
report={'tick_replay':args.tick_replay,'runs':reports,'core_sha256':hashlib.sha256(core.read_bytes()).hexdigest(),'common_ticks':len(common),'different_pad_ticks':len(different),'first_differences':[{'tick':t,'baseline':traces[0][t],'candidate':traces[1][t]} for t in different[:12]],'scope':'Instrumented 2P diagnostic builds; actual latched gameplay_pad at each game_update, no lost ring records. Frontend workload or explicit diagnostic pad override as marked by tick_replay; lives refilled. Replay asserts identical consumed pads, not identical whole-game state. Instrumentation affects timing; these are not production performance measurements or full gameplay state comparisons.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
