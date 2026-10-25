#!/usr/bin/env python3
"""Isolated low-overhead exact-input/refill workloads; not production controller validation."""
from pathlib import Path
import subprocess,shutil,json,hashlib,argparse
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--baseline-entity',type=Path);p.add_argument('--candidate-entity',type=Path);p.add_argument('--output-dir',type=Path);p.add_argument('--ticks',type=int,default=18000)
p.add_argument('--candidate-food-world',type=Path)
p.add_argument('--candidate-src-dir',type=Path,help='Overlay candidate C sources in the isolated build')
p.add_argument('--players',type=int,choices=(1,2))
a=p.parse_args()
if bool(a.baseline_entity)!=bool(a.candidate_entity):p.error('Specify both entity variants')
r=Path(__file__).resolve().parents[1];o=a.output_dir.resolve() if a.output_dir else r/'out/parity-audit/tick-replay-benchmark';o.mkdir(parents=True,exist_ok=True)
production=hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest();runs=[]
for variant in ('baseline','candidate'):
 d=o/variant;d.mkdir(exist_ok=True)
 for name in ('src','inc'):shutil.copytree(r/name,d/name,dirs_exist_ok=True)
 (d/'tools').mkdir(exist_ok=True)
 for name in ('build_native.py','verify_second_core.py','verify_libretro.c'):shutil.copy2(r/'tools'/name,d/'tools'/name)
 # Keep both historical sides explicit after production adopts the candidate.
 source=getattr(a,variant+'_entity') or r/f'out/parity-audit/actor-empty-tail/{variant}/entity.c'
 shutil.copy2(source,d/'src/entity.c')
 if variant=='candidate' and a.candidate_src_dir:
  for overlay in a.candidate_src_dir.glob('*.c'):shutil.copy2(overlay,d/'src'/overlay.name)
 if variant=='candidate' and a.candidate_food_world:shutil.copy2(a.candidate_food_world,d/'src/arcade_food_world.c')
 p=d/'src/game.c';s=p.read_text();needle='    g_frameCounter++;          /* headless FPS probe tick (see g_frameCounter) */';assert s.count(needle)==1
 s=s.replace(needle,needle+'''
    /* Diagnostic replay only: exact inputs and refill at simulation entry. */
    if(!g_attract && gameplay_input_ready){
        static u32 replay_step;
        u32 step=replay_step++;
        u16 pad=BUTTON_RIGHT | (step%16<3?BUTTON_A:0) | (step%200<24?BUTTON_UP:0) | (step%200>=24 && step%200<48?BUTTON_DOWN:0);
        gameplay_pad[0]=pad;gameplay_pad[1]=g_game.numPlayers>1?pad:0;
        g_game.players[0].lives=99;
        if(g_game.numPlayers>1)g_game.players[1].lives=99;
    }
''');p.write_text(s)
 p=d/'tools/verify_libretro.c';s=p.read_text();needle='else w8(ram,gg+k*42+32,99);';assert s.count(needle)==1;s=s.replace(needle,'else (void)k; /* source-tick diagnostic refill */');p.write_text(s)
 cache=(o/'baseline/out/native') if variant=='candidate' else r/'out/native'
 if cache.exists():shutil.copytree(cache,d/'out/native',dirs_exist_ok=True)
 subprocess.run(['python3',str(d/'tools/build_native.py')],check=True)
 with (d/'benchmark.log').open('w') as log:
  result=subprocess.run(['python3',str(d/'tools/verify_second_core.py'),'--ticks',str(a.ticks)]+(['--players',str(a.players)] if a.players else []),stdout=log,stderr=subprocess.STDOUT)
 assert result.returncode in (0,1)
 results=json.loads((d/'out/genplus-results.json').read_text());assert [x['players'] for x in results]==([a.players] if a.players else [1,2])
 if result.returncode:assert 'modes_with_missed_presentations' in (d/'benchmark.log').read_text()
 for x in results:
  assert a.ticks<=x['simulation_elapsed']<=a.ticks+1
  x['input_delivery']='Diagnostic game_update overrides pads from a zero-based active-gameplay source-tick counter; life refills occur at that boundary. Frontend input supplies boot/start only.'
  x['scope']='Low-overhead exact replay diagnostic, full rendering/audio and source pacing; lives refilled. No per-tick state recorder in this ROM. Not production-controller validation, full course completion or arcade parity proof.'
 runs.append({'variant':variant,'results':results})
 print(variant,[(x['players'],x['observed_presentation_hz'],x['worst_60_refresh_presentations'],x['final_pending_simulation_ticks']) for x in results],flush=True)
assert hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest()==production
report={'replay_origin':'zero-based first active gameplay update','candidate_src_dir':str(a.candidate_src_dir) if a.candidate_src_dir else None,'candidate_source_hashes':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(a.candidate_src_dir.glob('*.c'))} if a.candidate_src_dir else {},'candidate_food_world':str(a.candidate_food_world) if a.candidate_food_world else None,'production_rom_sha256':production,'runs':runs,'scope':'Two diagnostic source copies differ by the supplied entity variants and optional candidate source overlays. Same tick-indexed pad/refill rule, no bulk trace recording. Results retain rendering/audio/scheduler costs and diagnostic input overhead. Normal-controller stress results remain independent.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n')
