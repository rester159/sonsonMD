#!/usr/bin/env python3
"""Headless Genesis Plus GX frame-time tooling (diagnostic; never a release gate).

  bench   ROM_DIR [--players 1 2]     missed refreshes, worst 60-refresh window, debt
  profile [--players 2] [--exact]     build a -DPROF_HINT ROM into out/prof, sample the
          [--camera LO HI]            interrupted 68000 PC every 8th active line and
                                      report where single-tick frames spend their time
  compare BASE_DIR NEW_DIR            per-tick Plane A / linked-SAT / tile VRAM hash
          [--players 2]               equality between two -DREPLAY_EXACT_INPUT ROMs

Input is the verify_libretro.c script indexed by simulation tick. A normal ROM
latches the pad per display refresh, so builds with different timing can play
differently; build both sides with -DREPLAY_EXACT_INPUT for equal-work A/B.
Samples cover active display only (HInt cannot preempt VInt), and the profiler
itself costs ~2% CPU, so absolute miss counts are pessimistic."""
import argparse,bisect,collections,csv,os,pathlib,re,struct,subprocess
r=pathlib.Path(__file__).resolve().parents[1]
core=os.environ.get('GENPLUS_CORE',str(pathlib.Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'))
harness=r/'out/frame_profile_libretro'
def symbols(d):return {n:v for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)(?:\.lto_priv\.\d+)?\s*$',(d/'symbol.txt').read_text(),re.M)}
def run(d,players,ticks,env_extra={}):
    if not harness.exists() or harness.stat().st_mtime<(r/'tools/frame_profile_libretro.c').stat().st_mtime:
        subprocess.run(['cc','-O2','-o',str(harness),str(r/'tools/frame_profile_libretro.c')],check=True)
    s=symbols(d);env=os.environ.copy()
    env.update(SONSON_SIM_INPUT='1',SONSON_FIXED_TICKS='1',SONSON_CPULOAD=s['cpuFrameLoad'],HASH_OUT=str(d/f'hash-{players}p.csv'),**env_extra)
    for k,n in (('SONSON_PACING','session_pacing'),('PROF_PTR','prof_ptr'),('PROF_RING','prof_ring'),('PROF_CNT','prof_cnt')):
        if n in s:env[k]=s[n]
    env['PROF_OUT']=str(d/f'prof-{players}p.bin')
    out=subprocess.run([str(harness),core,str(d/'rom.bin'),str(players),str(ticks),*[s[n] for n in ('g_frameCounter','g_presentCounter','g_levelW','g_game','g_numPlayers','g_attract')],str(d/f'prof-{players}p.csv')],env=env,check=True,capture_output=True,text=True).stdout
    hz=float(re.search(r'CORE_TIMING fps=([0-9.]+)',out).group(1))
    rows=list(csv.DictReader((d/f'prof-{players}p.csv').open()));p=[int(x['present']) for x in rows]
    return rows,{'players':players,'missed':sum(a==b for a,b in zip(p,p[1:])),'worst60':min(p[i]-p[i-60] for i in range(60,len(p))),
        'max_debt':max(int(x['pending_ticks']) for x in rows),'present_hz':round((p[-1]-p[0])*hz/(len(p)-1),3),'refresh_hz':round(hz,3)}
def samples(d,players):
    data=(d/f'prof-{players}p.bin').read_bytes();o=0;out={}
    while o<len(data):
        f,_,n=struct.unpack_from('<3I',data,o);stamp=struct.unpack_from('<8H',data,o+12)
        out[f]=(stamp,[x&0xffffff for x in struct.unpack_from(f'<{n}I',data,o+28)]);o+=28+4*n
    return out
def namer(d):
    sy=sorted((int(v,16)&0xffffff,n) for v,t,n in re.findall(r'^([0-9a-fA-F]+)\s+(\w)\s+(\S+)\s*$',(d/'symbol.txt').read_text(),re.M)
              if t in 'tTwW' and int(v,16)<0x400000 and not n.startswith('profile_'))
    a=[x for x,_ in sy];return lambda pc:sy[bisect.bisect_right(a,pc)-1][1]
def bench(a):
    for pl in a.players:print(run(pathlib.Path(a.rom_dir).resolve(),pl,a.ticks)[1])
def profile(a):
    d=r/'out/prof'
    subprocess.run(['python3',str(r/'tools/build_native.py'),'--out','out/prof','-D','PROF_HINT',*(['-D','REPLAY_EXACT_INPUT'] if a.exact else [])],check=True,stdout=subprocess.DEVNULL)
    fn=namer(d)
    for pl in a.players:
        rows,res=run(d,pl,a.ticks);print(res);smp=samples(d,pl);by=collections.defaultdict(list)
        wait=lambda pc:fn(pc).startswith('VDP_waitVBlank')
        late=collections.Counter();normal=collections.Counter();nl=nn=0
        for x,y in zip(rows,rows[1:]):
            f=int(y['refresh']);dt=int(y['tick'])-int(x['tick']);missed=x['present']==y['present']
            if f not in smp or not(a.camera[0]<=int(y['camera'])<=a.camera[1]):continue
            stamp,pcs=smp[f];by[(dt,int(not missed))].append(sum(not wait(pc) for pc in pcs))
            if dt!=1:continue
            if missed or 200<=stamp[0]<224:nl+=1;target=late
            else:nn+=1;target=normal
            for pc in pcs:target[fn(pc)]+=1
        print('busy samples per refresh (28 = whole active display) by (ticks, presented):')
        for k in sorted(by):v=sorted(by[k]);print(f'  {k} n={len(v)} mean={sum(v)/len(v):.1f} p90={v[int(.9*len(v))]} p99={v[int(.99*len(v))]} max={v[-1]}')
        print(f'single-tick refreshes: {nn} normal, {nl} late (missed or ended past line 200); samples per refresh')
        for k in sorted(set(late)|set(normal),key=lambda k:-late[k]/max(nl,1))[:a.top]:
            print(f'  {normal[k]/max(nn,1):6.2f} {late[k]/max(nl,1):6.2f}  {k}')
def compare(a):
    base,new=pathlib.Path(a.base).resolve(),pathlib.Path(a.new).resolve()
    for pl in a.players:
        run(base,pl,a.ticks);run(new,pl,a.ticks)
        def load(d):
            rows=[l.strip().split(',') for l in (d/f'hash-{pl}p.csv').open()];out={}
            for x,y in zip(rows,rows[1:]):
                if y[2]!=x[2] and int(y[1])-int(x[1])==1:out.setdefault(int(y[1]),y[3:6])
            return out
        A,B=load(base),load(new);ks=sorted(set(A)&set(B))
        print(f'{pl}P ticks compared {len(ks)}: '+', '.join(f'{n} {100*sum(A[k][i]==B[k][i] for k in ks)/len(ks):.3f}%' for i,n in enumerate(('planeA','SAT','tiles'))))
p=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter);sub=p.add_subparsers(dest='cmd',required=True)
b=sub.add_parser('bench');b.add_argument('rom_dir');b.set_defaults(fn=bench)
q=sub.add_parser('profile');q.add_argument('--exact',action='store_true');q.add_argument('--camera',type=int,nargs=2,default=(0,1<<30));q.add_argument('--top',type=int,default=30);q.set_defaults(fn=profile)
c=sub.add_parser('compare');c.add_argument('base');c.add_argument('new');c.set_defaults(fn=compare)
for x in (b,q,c):x.add_argument('--players',type=int,nargs='+',choices=(1,2),default=[2]);x.add_argument('--ticks',type=int,default=55000)
a=p.parse_args();a.fn(a)
