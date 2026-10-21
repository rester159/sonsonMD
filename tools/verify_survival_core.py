#!/usr/bin/env python3
"""Genesis Plus GX headless stress at maximum survival pressure; injected grace flow."""
import pathlib,subprocess,os,re,csv,collections,json,hashlib
r=pathlib.Path(__file__).resolve().parents[1];o=r/'out/survival-core-check';o.mkdir(parents=True,exist_ok=True)
syms={n:v for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
core=pathlib.Path(os.environ.get('GENPLUS_CORE',str(pathlib.Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib')))
subprocess.run(['cc','-O2','-Wall','-Wextra',str(r/'tools/verify_libretro.c'),'-o',str(r/'out/verify_libretro')],check=True)
env=os.environ.copy();env.update(SONSON_SURVIVAL='1',SONSON_PRESSURE=syms['g_survivalPressure'],SONSON_ENTITIES=syms['g_entities'],SONSON_CPULOAD=syms['cpuFrameLoad'])
results=[]
for players in [1,2]:
 path=o/f'{players}p.csv'
 cmd=[str(r/'out/verify_libretro'),str(core),str(r/'out/rom.bin'),str(players),'7000',*[syms[n] for n in ['g_frameCounter','g_presentCounter','g_levelW','g_game','g_numPlayers','g_attract']],str(path)]
 subprocess.run(cmd,env=env,check=True,stdout=(o/f'{players}p.txt').open('w'),stderr=subprocess.STDOUT)
 rows=list(csv.DictReader(path.open()))[1000:];ps=[int(v['present']) for v in rows];hist=dict(collections.Counter(b-a for a,b in zip(ps,ps[1:])))
 result={'players':players,'measured_refreshes':len(rows),'presentation_deltas':hist,'cpu_peak_percent':max(int(v['cpu']) for v in rows)*100/2048,'max_entities':max(int(v['entities']) for v in rows),'max_scheduled':max(int(v['scheduled']) for v in rows),'rom_sha256':hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest(),'core_sha256':hashlib.sha256(core.read_bytes()).hexdigest()}
 print(result,flush=True);results.append(result)
 assert len(rows)==6000 and hist=={1:5999} and result['max_scheduled']==16
(o/'results.json').write_text(json.dumps(results,indent=2)+'\n')
