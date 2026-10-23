#!/usr/bin/env python3
"""Compare per-tick fields and sampled native pools with exact diagnostic replay."""
from pathlib import Path
import shutil,subprocess,re,json,hashlib,csv,os,argparse
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("--tick-replay",action="store_true");parser.add_argument("--ticks",type=int,default=18000);parser.add_argument('--baseline-entity',type=Path);parser.add_argument('--candidate-entity',type=Path);parser.add_argument('--output-dir',type=Path)
parser.add_argument('--candidate-lto',action='store_true')
parser.add_argument('--candidate-builder',type=Path,help='Alternate build driver for an isolated compiler regression check')
parser.add_argument('--candidate-src-dir',type=Path,help='Candidate C source overlays for an isolated subsystem regression')
parser.add_argument('--candidate-game',type=Path,help='Optional candidate game.c overlay for isolated loop regressions')
parser.add_argument('--candidate-opt',choices=('2','3'))
parser.add_argument('--baseline-opt',choices=('2','3'))
parser.add_argument('--survival',action='store_true',help='Source-tick pressure/grace fixture in Home Survival')
args=parser.parse_args();args.tick_replay=True
if bool(args.baseline_entity)!=bool(args.candidate_entity):parser.error('Specify both entity variants')
r=Path(__file__).resolve().parents[1];o=args.output_dir.resolve() if args.output_dir else r/'out/parity-audit/pool-state-replay';o.mkdir(parents=True,exist_ok=True)
core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
reports=[];traces=[];pool_traces=[];refresh_times=[]
pool_objects=['g_entities','arcade_waves', 'arcade_rng', 'arcade_foes', 'arcade_weapons', 'arcade_world', 'arcade_large_actors', 'arcade_k_actors', 'arcade_i_actors', 'arcade_i_wave', 'arcade_main', 'arcade_c', 'arcade_d', 'arcade_e', 'arcade_f', 'arcade_bonus_next', 'arcade_bonus_limit', 'arcade_i_live', 'arcade_i_reservation', 'arcade_shot_budget', 'arcade_e_ready', 'arcade_main_ready']
for variant in ('baseline','candidate'):
 d=o/variant;d.mkdir(exist_ok=True)
 for name in ('src','inc'):shutil.copytree(r/name,d/name,dirs_exist_ok=True)
 (d/'tools').mkdir(exist_ok=True);shutil.copy2(r/'tools/build_native.py',d/'tools/build_native.py')
 if variant=='candidate' and args.candidate_builder:shutil.copy2(args.candidate_builder,d/'tools/build_native.py')
 # Keep both historical sides explicit after production adopts the candidate.
 source=getattr(args,variant+'_entity') or r/f'out/parity-audit/actor-empty-tail/{variant}/entity.c'
 shutil.copy2(source,d/'src/entity.c')
 if variant=='candidate' and args.candidate_src_dir:
  for overlay in args.candidate_src_dir.glob('*.c'):shutil.copy2(overlay,d/'src'/overlay.name)
 if variant=='candidate' and args.candidate_game:shutil.copy2(args.candidate_game,d/'src/game.c')
 p=d/'src/game.c';s=p.read_text();s=s.replace('bool game_update(void)\n{','void diagnostic_pool_snapshot(u32 tick);\nvoid diagnostic_actor_snapshot(volatile u32 *out);\nvolatile u32 diagnostic_input_count;\nvolatile u32 diagnostic_input_rows[64][34];\nbool game_update(void)\n{',1)
 needle='    g_frameCounter++;          /* headless FPS probe tick (see g_frameCounter) */'
 assert s.count(needle)==1
 s=s.replace(needle,needle+'''
    if(!g_attract && gameplay_input_ready){
        u16 row=(u16)(diagnostic_input_count&63);
        diagnostic_input_rows[row][0]=g_frameCounter;
        diagnostic_input_rows[row][1]=((u32)gameplay_pad[0]<<16)|gameplay_pad[1];
        diagnostic_input_rows[row][2]=entity_arcade_clock();
        diagnostic_input_rows[row][3]=(u16)g_game.cameraX;
        {u16 k;for(k=0;k<2;k++){
            const Player *p=&g_game.players[k];
            diagnostic_input_rows[row][4+k*3]=((u32)(u16)p->x<<16)|(u16)p->y;
            diagnostic_input_rows[row][5+k*3]=((u32)p->flow<<24)|((u32)p->lives<<16)|p->flowTimer;
            diagnostic_input_rows[row][6+k*3]=p->score;
        }}
        diagnostic_actor_snapshot(&diagnostic_input_rows[row][10]);
        if((g_frameCounter&63)==0)diagnostic_pool_snapshot(g_frameCounter);
        diagnostic_input_count++;
    }
''')
 if args.tick_replay:
  s=s.replace('        u16 row=(u16)(diagnostic_input_count&63);', '''        g_game.players[0].lives=g_game.players[1].lives=99;
        u32 step=g_frameCounter-1;
        u16 pad=BUTTON_RIGHT | (step%16<3?BUTTON_A:0) | (step%200<24?BUTTON_UP:0) | (step%200>=24 && step%200<48?BUTTON_DOWN:0);
        gameplay_pad[0]=gameplay_pad[1]=pad;
        u16 row=(u16)(diagnostic_input_count&63);''')
 if args.survival:
  s=s.replace('        u16 row=(u16)(diagnostic_input_count&63);', '        g_survivalPressure=6;g_game.players[0].flow=g_game.players[1].flow=FLOW_RESPAWNING;g_game.players[0].flowTimer=g_game.players[1].flowTimer=60000;\n        u16 row=(u16)(diagnostic_input_count&63);')
 p.write_text(s)
 with (d/'src/entity.c').open('a') as f:f.write("""

volatile u8 diagnostic_pool_rows[2][4096];
volatile u32 diagnostic_pool_ticks[2],diagnostic_pool_count;
volatile u16 diagnostic_pool_size;
void diagnostic_pool_snapshot(u32 tick){
    u16 slot=(u16)(diagnostic_pool_count&1),offset=0;
    memcpy((void*)&diagnostic_pool_rows[slot][offset],g_entities,sizeof(g_entities));offset+=sizeof(g_entities);
    memcpy((void*)&diagnostic_pool_rows[slot][offset],&arcade_waves,sizeof(arcade_waves));offset+=sizeof(arcade_waves);
    memcpy((void*)&diagnostic_pool_rows[slot][offset],&arcade_rng,sizeof(arcade_rng));offset+=sizeof(arcade_rng);
    memcpy((void*)&diagnostic_pool_rows[slot][offset],&arcade_foes,sizeof(arcade_foes));offset+=sizeof(arcade_foes);
    memcpy((void*)&diagnostic_pool_rows[slot][offset],&arcade_weapons,sizeof(arcade_weapons));offset+=sizeof(arcade_weapons);
    memcpy((void*)&diagnostic_pool_rows[slot][offset],&arcade_world,sizeof(arcade_world));offset+=sizeof(arcade_world);
    memcpy((void*)&diagnostic_pool_rows[slot][offset],&arcade_large_actors,sizeof(arcade_large_actors));offset+=sizeof(arcade_large_actors);
    memcpy((void*)&diagnostic_pool_rows[slot][offset],&arcade_k_actors,sizeof(arcade_k_actors));offset+=sizeof(arcade_k_actors);
    memcpy((void*)&diagnostic_pool_rows[slot][offset],&arcade_i_actors,sizeof(arcade_i_actors));offset+=sizeof(arcade_i_actors);
    memcpy((void*)&diagnostic_pool_rows[slot][offset],&arcade_i_wave,sizeof(arcade_i_wave));offset+=sizeof(arcade_i_wave);
    memcpy((void*)&diagnostic_pool_rows[slot][offset],&arcade_main,sizeof(arcade_main));offset+=sizeof(arcade_main);
    memcpy((void*)&diagnostic_pool_rows[slot][offset],&arcade_c,sizeof(arcade_c));offset+=sizeof(arcade_c);
    memcpy((void*)&diagnostic_pool_rows[slot][offset],&arcade_d,sizeof(arcade_d));offset+=sizeof(arcade_d);
    memcpy((void*)&diagnostic_pool_rows[slot][offset],&arcade_e,sizeof(arcade_e));offset+=sizeof(arcade_e);
    memcpy((void*)&diagnostic_pool_rows[slot][offset],&arcade_f,sizeof(arcade_f));offset+=sizeof(arcade_f);
    memcpy((void*)&diagnostic_pool_rows[slot][offset],&arcade_bonus_next,sizeof(arcade_bonus_next));offset+=sizeof(arcade_bonus_next);
    memcpy((void*)&diagnostic_pool_rows[slot][offset],&arcade_bonus_limit,sizeof(arcade_bonus_limit));offset+=sizeof(arcade_bonus_limit);
    memcpy((void*)&diagnostic_pool_rows[slot][offset],&arcade_i_live,sizeof(arcade_i_live));offset+=sizeof(arcade_i_live);
    memcpy((void*)&diagnostic_pool_rows[slot][offset],&arcade_i_reservation,sizeof(arcade_i_reservation));offset+=sizeof(arcade_i_reservation);
    memcpy((void*)&diagnostic_pool_rows[slot][offset],&arcade_shot_budget,sizeof(arcade_shot_budget));offset+=sizeof(arcade_shot_budget);
    memcpy((void*)&diagnostic_pool_rows[slot][offset],&arcade_e_ready,sizeof(arcade_e_ready));offset+=sizeof(arcade_e_ready);
    memcpy((void*)&diagnostic_pool_rows[slot][offset],&arcade_main_ready,sizeof(arcade_main_ready));offset+=sizeof(arcade_main_ready);
    diagnostic_pool_size=offset;diagnostic_pool_ticks[slot]=tick;diagnostic_pool_count++;
}
typedef char diagnostic_pool_fits[(sizeof(g_entities)+sizeof(arcade_waves)+sizeof(arcade_rng)+sizeof(arcade_foes)+sizeof(arcade_weapons)+sizeof(arcade_world)+sizeof(arcade_large_actors)+sizeof(arcade_k_actors)+sizeof(arcade_i_actors)+sizeof(arcade_i_wave)+sizeof(arcade_main)+sizeof(arcade_c)+sizeof(arcade_d)+sizeof(arcade_e)+sizeof(arcade_f)+sizeof(arcade_bonus_next)+sizeof(arcade_bonus_limit)+sizeof(arcade_i_live)+sizeof(arcade_i_reservation)+sizeof(arcade_shot_budget)+sizeof(arcade_e_ready)+sizeof(arcade_main_ready)<=4096)?1:-1];
void diagnostic_actor_snapshot(volatile u32 *out){
    u16 k;for(k=0;k<12;k++){
        const ArcadeFormationActor *a=&arcade_foes[k].body;
        out[k*2]=((u32)a->x<<16)|((u16)a->xf<<8)|a->y;
        out[k*2+1]=((u32)a->active<<24)|((u32)a->state<<16)|((u16)a->phase<<8)|a->yf;
    }
}
""")
 cache=(o/'baseline/out/native') if variant=='candidate' else r/'out/native'
 if cache.exists():shutil.copytree(cache,d/'out/native',dirs_exist_ok=True)
 subprocess.run(['python3',str(d/'tools/build_native.py')]+([('--lto' if variant=='candidate' else '--no-lto')] if args.candidate_lto else [])+(['--opt',getattr(args,variant+'_opt')] if getattr(args,variant+'_opt') else []),check=True)
 syms={n:v for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)(?:\.lto_priv\.\d+)?\s*$',(d/'out/symbol.txt').read_text(),re.M)}
 s=(r/'tools/verify_libretro.c').read_text().replace('else w8(ram,gg+k*42+32,99);','else (void)k; /* diagnostic refill occurs at source tick */')
 if args.survival:
  grace='if(survival) {w8(ram,pressure_addr,6);w8(ram,gg+k*42+34,2);w8(ram,gg+k*42+36,0xEA);w8(ram,gg+k*42+37,0x60);}'
  assert grace in s;s=s.replace(grace,'if(survival) {(void)k; /* grace injected at source tick */}')
 s=s.replace('    for(frame=0;',f'''    unsigned pool_seen=0;FILE *pools=fopen("{d}/pools.csv","w");if(!pools)return 3;
    unsigned seen=0;FILE *inputs=fopen("{d}/inputs.csv","w");if(!inputs)return 3;
    fputs("tick,pad0,pad1,clock,refresh,state0,state1,state2,state3,state4,state5,state6,state7,state8,state9,state10,state11,state12,state13,state14,state15,state16,state17,state18,state19,state20,state21,state22,state23,state24,state25,state26,state27,state28,state29,state30\\n",inputs);
    for(frame=0;''',1)
 s=s.replace('        retro_run();',f'''        retro_run();
        unsigned count=r32(ram,0x{syms['diagnostic_input_count']});
        if(count-seen>64)return 4;
        while(seen<count){{
            unsigned address=0x{syms['diagnostic_input_rows']} + (seen&63)*136;
            unsigned pad=r32(ram,address+4);
            fprintf(inputs,"%u,%u,%u,%u,%u",r32(ram,address),pad>>16,pad&65535,r32(ram,address+8),frame);
            for(unsigned k=3;k<34;k++)fprintf(inputs,",%u",r32(ram,address+k*4));
            fputc('\\n',inputs);seen++;
        }}
        unsigned pool_count=r32(ram,0x{syms['diagnostic_pool_count']});
        if(pool_count-pool_seen>2)return 5;
        while(pool_seen<pool_count){{
            unsigned slot=pool_seen&1,size=r16(ram,0x{syms['diagnostic_pool_size']});
            if(size>4096)return 6;
            fprintf(pools,"%u,",r32(ram,0x{syms['diagnostic_pool_ticks']} + slot*4));
            for(unsigned k=0;k<size;k++)fprintf(pools,"%02x",r8(ram,0x{syms['diagnostic_pool_rows']} + slot*4096+k));
            fputc('\\n',pools);pool_seen++;
        }}''',1)
 s=s.replace('    fclose(f);printf(', '    fclose(pools);fclose(inputs);fclose(f);printf(',1)
 (d/'harness.c').write_text(s);subprocess.run(['cc','-O2',str(d/'harness.c'),'-o',str(d/'harness')],check=True)
 env=os.environ.copy();env.update(SONSON_SIM_INPUT='1',SONSON_FIXED_TICKS='1')
 if args.survival:env.update(SONSON_SURVIVAL='1',SONSON_PRESSURE=syms['g_survivalPressure'])
 cmd=[str(d/'harness'),str(core),str(d/'out/rom.bin'),'2',str(args.ticks),*[syms[n] for n in ('g_frameCounter','g_presentCounter','g_levelW','g_game','g_numPlayers','g_attract')],str(d/'refresh.csv')]
 with (d/'runtime.log').open('w') as log:subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
 pool_traces.append({int(t):bytes.fromhex(b) for t,b in csv.reader((d/'pools.csv').open())})
 rows=list(csv.DictReader((d/'inputs.csv').open()));assert len(rows)>args.ticks
 refresh_times.append({int(x['tick']):int(x['refresh']) for x in rows})
 ticks=[int(x['tick']) for x in rows];assert all(b==a+1 for a,b in zip(ticks,ticks[1:])), 'Missing simulation records'
 traces.append({int(x['tick']):(int(x['pad0']),int(x['pad1']),int(x['clock']),*[int(x['state'+str(k)]) for k in range(31)]) for x in rows})
 reports.append({'variant':variant,'records':len(rows),'hashes':{str(p.relative_to(d)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (d/'out/rom.bin',d/'out/symbol.txt',d/'harness.c',d/'inputs.csv',d/'pools.csv')}})
common=sorted(traces[0].keys()&traces[1].keys());different=[t for t in common if traces[0][t][:2]!=traces[1][t][:2]]
if args.tick_replay:assert not different, different[:12]
state_different=[t for t in common if traces[0][t][2:]!=traces[1][t][2:]]
fields=['pad0','pad1','clock','camera','p0_xy','p0_flow_lives_timer','p0_score','p1_xy','p1_flow_lives_timer','p1_score']+[f'gh{k}_{part}' for k in range(12) for part in ('x_xf_y','active_state_phase_yf')]
pool_common=sorted(pool_traces[0].keys()&pool_traces[1].keys());assert pool_common
pool_different=[t for t in pool_common if pool_traces[0][t]!=pool_traces[1][t]]
report={'candidate_src_dir':str(args.candidate_src_dir) if args.candidate_src_dir else None,'candidate_builder':str(args.candidate_builder) if args.candidate_builder else None,'candidate_game':str(args.candidate_game) if args.candidate_game else None,'survival_fixture':args.survival,'baseline_opt':args.baseline_opt,'candidate_opt':args.candidate_opt,'candidate_lto':args.candidate_lto,'pool_objects':pool_objects,'pool_samples_compared':len(pool_common),'pool_different_ticks':len(pool_different),'first_pool_differences':[{'tick':t,'byte_offsets':[k for k,(a,b) in enumerate(zip(pool_traces[0][t],pool_traces[1][t])) if a!=b]} for t in pool_different[:5]],'field_names':fields,'life_refill_clock':'simulation','requested_ticks':args.ticks,'state_different_ticks':len(state_different),'first_state_differences':[{'tick':t,'fields':[{'index':k,'name':fields[k],'baseline':a,'candidate':b} for k,(a,b) in enumerate(zip(traces[0][t],traces[1][t])) if a!=b]} for t in state_different[:12]],'tick_replay':args.tick_replay,'runs':reports,'core_sha256':hashlib.sha256(core.read_bytes()).hexdigest(),'common_ticks':len(common),'different_pad_ticks':len(different),'first_differences':[{'tick':t,'baseline':traces[0][t],'candidate':traces[1][t]} for t in different[:12]],'scope':'Instrumented 2P diagnostic builds; actual latched gameplay_pad at each game_update, no lost ring records. Frontend workload or explicit diagnostic pad override as marked by tick_replay; lives refilled deterministically at each source tick, with frontend refills disabled. Replay asserts identical consumed pads; compares clock, camera, player XY/flow/lives/timer/score and G/H XY/fractions/active/state/phase. Every 64 ticks also compares raw bytes of the listed native pools/controllers/RNG/flags, including padding. Not all game globals, renderer state, food/background objects or audio state are covered. Snapshots precede current game_update, following prior rendering/catch-up admission. Instrumentation affects timing; these are not production performance measurements or full gameplay state comparisons.'}
first,last=pool_common[0],pool_common[-1]
report['pool_snapshot_bytes']=len(pool_traces[0][first])
assert all(len(blob)==report['pool_snapshot_bytes'] for run in pool_traces for blob in run.values())
report['unmatched_tail_records']={v:len(traces[i].keys()-set(common)) for i,v in enumerate(('baseline','candidate'))}
report['matched_interval_timing']=[{'variant':v,'first_tick':first,'last_tick':last,'source_tick_intervals':last-first,'elapsed_refresh_intervals':refresh_times[i][last]-refresh_times[i][first]} for i,v in enumerate(('baseline','candidate'))]
report['matched_interval_timing_scope']='Instrumented diagnostic builds. Equal tick interval; consult state differences before attributing timing. Snapshot drains quantize endpoints to refresh boundaries. Includes recording/render/audio/wait overhead, not production FPS or isolated CPU cycles.'
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
