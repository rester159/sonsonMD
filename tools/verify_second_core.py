#!/usr/bin/env python3
"""Run both player modes in Genesis Plus GX with no window or frontend."""
import pathlib,re,subprocess,csv,collections,json,hashlib,os,argparse
p=argparse.ArgumentParser();p.add_argument("--frames",type=int,default=42000);p.add_argument("--players",type=int,choices=[1,2]);p.add_argument("--ticks",type=int);args=p.parse_args()
if args.ticks is not None and args.ticks<61:p.error("--ticks must be at least 61")
limit=args.ticks if args.ticks is not None else args.frames
if args.frames<61:p.error("--frames must be at least 61 for the 60-refresh window metric")
root=pathlib.Path(__file__).resolve().parents[1];out=root/'out';core=pathlib.Path(os.environ.get('GENPLUS_CORE',str(pathlib.Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib')))
subprocess.run(['cc','-O2','-Wall','-Wextra',str(root/'tools/verify_libretro.c'),'-o',str(out/'verify_libretro')],check=True)
syms={n:v for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)(?:\.lto_priv\.\d+)?\s*$',(out/'symbol.txt').read_text(),re.M)}
env=os.environ.copy();env.update(SONSON_F=syms["arcade_f"],SONSON_E=syms["arcade_e"],SONSON_D=syms["arcade_d"],SONSON_C=syms["arcade_c"],SONSON_ADMISSION=syms["arcade_admission_cache"],SONSON_MAIN=syms["arcade_main"],SONSON_ENTITIES=syms["g_entities"],SONSON_CPULOAD=syms["cpuFrameLoad"],SONSON_LARGE=syms["arcade_large_actors"],SONSON_KPOOL=syms["arcade_k_actors"])
# New builds expose the shared Arcade owner; older builds used a direct
# gameplay object. Never treat the new gameplay pointer as the object itself.
if "session_pacing" in syms:env["SONSON_PACING"]=syms["session_pacing"]
elif "gameplay_pacing" in syms:env["SONSON_PACING"]=syms["gameplay_pacing"]
if args.ticks is not None:env.update(SONSON_SIM_INPUT="1",SONSON_FIXED_TICKS="1")
else:
    env.pop("SONSON_SIM_INPUT",None);env.pop("SONSON_FIXED_TICKS",None)
rom_hash=hashlib.sha256((out/"rom.bin").read_bytes()).hexdigest()
symbol_hash=hashlib.sha256((out/'symbol.txt').read_bytes()).hexdigest()
# A failed startup must not leave a prior ROM's success report in place.
(out/'genplus-results.json').unlink(missing_ok=True)
results=[];failures=[]
for players in ([args.players] if args.players else [1,2]):
    path=out/f'genplus-{players}p.csv'
    command=[str(out/'verify_libretro'),str(core),str(out/'rom.bin'),str(players),str(limit),*[syms[s] for s in ['g_frameCounter','g_presentCounter','g_levelW','g_game','g_numPlayers','g_attract']],str(path)]
    subprocess.run(command,env=env,check=True,stdout=(out/f'genplus-{players}p.txt').open('w'),stderr=subprocess.STDOUT)
    rows=list(csv.DictReader(path.open()));p=[int(r['present']) for r in rows]
    result={'players':players,'input_clock':'simulation' if args.ticks is not None else 'refresh','requested_ticks':args.ticks,'simulation_elapsed':int(rows[-1]['simulation_elapsed']),'samples':len(rows),'presentation_delta_histogram':dict(collections.Counter(b-a for a,b in zip(p,p[1:]))),'worst_60_refresh_presentations':min(p[i]-p[i-60] for i in range(60,len(p))),'cpu_peak_percent':max(int(v['cpu'])for v in rows)*100/2048,'admission_cache_hits':int(rows[-1]['admission_hits'])-int(rows[0]['admission_hits']),'admission_cache_misses':int(rows[-1]['admission_misses'])-int(rows[0]['admission_misses']),'max_f_enemies':max(int(v['f_enemies'])for v in rows),'max_e_enemies':max(int(v['e_enemies'])for v in rows),'max_d_enemies':max(int(v['d_enemies'])for v in rows),'max_c_enemies':max(int(v['c_enemies'])for v in rows),'max_main_enemies':max(int(v['main_enemies'])for v in rows),'max_entities':max(int(v['entities'])for v in rows),'max_scheduled':max(int(v['scheduled'])for v in rows),'max_large':max(int(v['large'])for v in rows),'max_k_projectiles':max(int(v['k_projectiles'])for v in rows),'refreshes_with_large':sum(int(v['large'])>0 for v in rows),'scope':'Controller-driven workload with lives refilled; see ribbon_wraps for course coverage; AI remains partial, not final equivalent source workload','ribbon_wraps':sum(int(b['camera'])<int(a['camera']) for a,b in zip(rows,rows[1:])),'rom_sha256':rom_hash,'core_sha256':hashlib.sha256(core.read_bytes()).hexdigest()}
    timing=re.search(r'CORE_TIMING fps=([0-9.]+) sample_rate=([0-9.]+)',(out/f'genplus-{players}p.txt').read_text())
    assert timing, 'Core timing metadata missing'
    hz=float(timing.group(1));assert 40<hz<70
    intervals=len(rows)-1
    result.update(core_refresh_hz=hz,core_sample_rate=float(timing.group(2)),
        sampled_seconds=intervals/hz,
        observed_simulation_hz=(int(rows[-1]['tick'])-int(rows[0]['tick']))*hz/intervals,
        observed_presentation_hz=(p[-1]-p[0])*hz/intervals)
    result["input_delivery"]='Frontend samples a simulation-indexed script at refresh boundaries; game latches input before catch-up. Equal tick counts do not imply identical consumed input or trajectories.' if args.ticks is not None else 'Refresh-indexed frontend input; game latches before simulation catch-up.'
    result["sampling_limit_overshoot"]=int(rows[-1]["simulation_elapsed"])-args.ticks if args.ticks is not None else None
    result.update(max_pending_simulation_ticks=max(int(v['pending_ticks']) for v in rows),
        final_pending_simulation_ticks=int(rows[-1]['pending_ticks']),
        cadence_debt_scope='Refresh-boundary snapshots; positive persistent/growing debt means source timing is not maintained.')
    print(result,flush=True);results.append(result)
    (out/'genplus-results.json').write_text(json.dumps(results,indent=2)+'\n')
    assert hashlib.sha256((out/'rom.bin').read_bytes()).hexdigest()==rom_hash
    assert hashlib.sha256((out/'symbol.txt').read_bytes()).hexdigest()==symbol_hash
    assert set(int(r['players']) for r in rows)=={players}
    if args.ticks is None:assert len(rows)==args.frames
    else:
        # Two source updates can complete between refresh-boundary observations.
        # Preserve/report the observed work; never truncate CSVs to hide overshoot.
        assert args.ticks<=int(rows[-1]['simulation_elapsed'])<=args.ticks+1
    if result['presentation_delta_histogram']!={1:len(rows)-1}:failures.append(players)
(out/'genplus-results.json').write_text(json.dumps(results,indent=2)+'\n')

assert not failures,{'modes_with_missed_presentations':failures}
