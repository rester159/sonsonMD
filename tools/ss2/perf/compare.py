#!/usr/bin/env python3
"""Compare matched completed-tick evidence: BEFORE_DIR AFTER_DIR CORE_FPS.

Uses ticks 1000..6000. The presentation counter counts pictures prepared
for DMA, not independently observed fresh scanouts. Reports both fixed-work
cost and rolling throughput; refuses a divergent route.
"""
import csv,json,sys
from pathlib import Path
fps=float(sys.argv[3]);data=[]
for path in sys.argv[1:3]:
    rows=list(csv.DictReader((Path(path)/'work.csv').open()));by_tick={}
    for row in rows: by_tick.setdefault(int(row['tick']),row)
    data.append((rows,by_tick,Path(path)))
fields=('camx','camy','px','py','rng','objects')
common=sorted(k for k in data[0][1].keys()&data[1][1].keys() if k>=60)
bad=[k for k in common if any(data[0][1][k][f]!=data[1][1][k][f] for f in fields)]
assert not bad,('Divergent completed state',bad[:10])
report={'core_fps':fps,'matched_completed_ticks':len(common),'workload_fields':fields,'mismatches':len(bad),'tick_window':[1000,6000],'runs':[]}
for rows,by_tick,path in data:
    a,b=by_tick[1000],by_tick[6000];start,end=int(a['refresh']),int(b['refresh'])
    selected=[r for r in rows if start<=int(r['refresh'])<=end]
    trace=[r for r in csv.DictReader((path/'trace.csv').open()) if start<=int(r['frame'])<=end]
    delta=[(int(y['pres'])-int(x['pres']))&65535 for x,y in zip(trace,trace[1:])]
    gap=0;gaps=[]
    for d in delta:
        if d:gaps.append(gap+1);gap=0
        else:gap+=1
    count=(int(b['pres'])-int(a['pres']))&65535
    report['runs'].append({'refreshes_for_5000_ticks':end-start,'prepared_presentations':count,
        'simulation_hz':5000*fps/(end-start),'prepared_presentations_per_second':count*fps/(end-start),
        'worst_60_refresh_prepared_count':min(sum(delta[i:i+60]) for i in range(len(delta)-59)),
        'longest_prepared_gap_refreshes':max(gaps+[gap]),'peak_live_objects':max(int(r['objects']) for r in selected)})
print(json.dumps(report,indent=2))
