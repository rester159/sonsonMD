#!/usr/bin/env python3
"""Run both player modes in Genesis Plus GX with no window or frontend."""
import pathlib,re,subprocess,csv,collections,json,hashlib,os,argparse
p=argparse.ArgumentParser();p.add_argument("--frames",type=int,default=42000);args=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[1];out=root/'out';core=pathlib.Path(os.environ.get('GENPLUS_CORE',str(pathlib.Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib')))
subprocess.run(['cc','-O2','-Wall','-Wextra',str(root/'tools/verify_libretro.c'),'-o',str(out/'verify_libretro')],check=True)
syms={n:v for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(out/'symbol.txt').read_text(),re.M)}
results=[]
for players in [1,2]:
    path=out/f'genplus-{players}p.csv'
    command=[str(out/'verify_libretro'),str(core),str(out/'rom.bin'),str(players),str(args.frames),*[syms[s] for s in ['g_frameCounter','g_presentCounter','g_levelW','g_game','g_numPlayers','g_attract']],str(path)]
    subprocess.run(command,check=True,stdout=(out/f'genplus-{players}p.txt').open('w'),stderr=subprocess.STDOUT)
    rows=list(csv.DictReader(path.open()));p=[int(r['present']) for r in rows]
    result={'players':players,'samples':len(rows),'presentation_delta_histogram':dict(collections.Counter(b-a for a,b in zip(p,p[1:]))),'worst_60_refresh_presentations':min(p[i]-p[i-60] for i in range(60,len(p))),'ribbon_wraps':sum(int(b['camera'])<int(a['camera']) for a,b in zip(rows,rows[1:])),'rom_sha256':hashlib.sha256((out/'rom.bin').read_bytes()).hexdigest(),'core_sha256':hashlib.sha256(core.read_bytes()).hexdigest()}
    print(result,flush=True);results.append(result)
    assert len(rows)==args.frames and set(int(r['players']) for r in rows)=={players}
    assert result['presentation_delta_histogram']=={1:args.frames-1}
(out/'genplus-results.json').write_text(json.dumps(results,indent=2)+'\n')
