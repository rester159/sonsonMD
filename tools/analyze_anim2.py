#!/usr/bin/env python3
"""analyze_anim2.py -- focused extraction of the PLAYER and ENEMY animation
cycles from the spriteram capture. Builds tracks (as analyze_anim) then, for the
player (bank 0/1) and enemy (bank 2), classifies each frame's STATE from motion
and prints the dominant repeating code-cycles + cadence per state.

States inferred from motion deltas along a track:
  player: RUN   = |dx|>=1 and dy==0 (running along a lane)
          CLIMB = dy!=0 (lane change / on ladder)  [y steps]
          IDLE  = dx==0 and dy==0
  enemy : WALK  = moving; DEATH = the terminal held-frame burst (codes jump to a
          distinct set and x stops).
We don't *force* arcade-exactness from motion alone; we report the raw dominant
cycles so a human picks the canonical sequence. The ROM cross-check (gen_anim)
is the tie-breaker.
"""
import sys
from collections import defaultdict, Counter

RAW = r"D:/portForge/tools/mame/ss_anim_cap_raw.txt"

def load():
    obs = []
    for line in open(RAW):
        line=line.strip()
        if not line or line.startswith("#"): continue
        p=line.split()
        if len(p)<8: continue
        obs.append(tuple(int(v) for v in p[:8]))
    return obs

def build_tracks(obs):
    frames=sorted(set(o[0] for o in obs))
    per=defaultdict(list)
    for o in obs: per[o[0]].append(o)
    tracks=[]; live=[]
    for fr in frames:
        cur=per[fr]; used=[False]*len(cur); newlive=[]
        for ti,last in live:
            best=-1; bestd=999
            for i,o in enumerate(cur):
                if used[i] or o[3]!=last[3]: continue
                dx=abs(o[4]-last[4]); dy=abs(o[5]-last[5])
                if dx>200: dx=256-dx
                if dx<=7 and dy<=12:
                    d=dx+dy
                    if d<bestd: bestd=d;best=i
            if best>=0:
                used[best]=True; tracks[ti].append(cur[best]); newlive.append((ti,cur[best]))
        for i,o in enumerate(cur):
            if used[i]: continue
            tracks.append([o]); newlive.append((len(tracks)-1,cur[best] if False else o))
        live=newlive
    return tracks

def transitions(track):
    """Return list of (code, dx, dy) per step (dx,dy vs previous obs)."""
    out=[]
    for i,o in enumerate(track):
        if i==0: out.append((o[2],0,0,o[6])); continue
        dx=o[4]-track[i-1][4]; dy=o[5]-track[i-1][5]
        if dx>200: dx-=256
        if dx<-200: dx+=256
        out.append((o[2],dx,dy,o[6]))
    return out

def cycles_for_state(tracks, banks, statefn, label):
    """Aggregate code-run sequences across all tracks of the given banks while
    statefn(dx,dy) holds. Print the most common ordered code n-grams."""
    # Build, per track, the maximal runs where statefn holds; within a run,
    # compress to (code,dur) and accumulate the ORDERED distinct-code cycle.
    seqs=[]
    perframe_codes=Counter()
    durations=defaultdict(list)   # code -> list of hold durations in this state
    for t in tracks:
        if t[0][3] not in banks: continue
        tr=transitions(t)
        run=[]
        def flush_run(run):
            if len(run)<4: return
            # compress
            comp=[]
            for c in run:
                if comp and comp[-1][0]==c: comp[-1][1]+=1
                else: comp.append([c,1])
            for c,n in comp:
                durations[c].append(n)
            seqs.append([c for c,_ in comp])
        for (c,dx,dy,fx) in tr:
            if statefn(dx,dy):
                run.append(c); perframe_codes[c]+=1
            else:
                flush_run(run); run=[]
        flush_run(run)
    print("\n=== %s (banks %s) ===" % (label, sorted(banks)))
    print("  frames-in-state per code:", dict(perframe_codes.most_common()))
    print("  median hold (frames) per code:",
          {c: sorted(durations[c])[len(durations[c])//2] for c in sorted(durations) if durations[c]})
    # most common ordered cycle: take the distinct-code order of the longest seqs
    # find the modal short cycle by scanning bigrams
    bg=Counter()
    for s in seqs:
        for i in range(len(s)-1):
            bg[(s[i],s[i+1])]+=1
    print("  top code transitions (a->b : count):")
    for (a,b),n in bg.most_common(12):
        print("     %d -> %d : %d" % (a,b,n))

def main():
    obs=load()
    tracks=build_tracks(obs)
    long=[t for t in tracks if len(t)>=40]
    print("tracks>=40:",len(long))

    # PLAYER: banks 0 and 1
    cycles_for_state(long, {0,1}, lambda dx,dy: dy==0 and abs(dx)>=1, "PLAYER RUN (dy==0, moving)")
    cycles_for_state(long, {0,1}, lambda dx,dy: dy!=0, "PLAYER CLIMB (dy!=0)")
    cycles_for_state(long, {0,1}, lambda dx,dy: dx==0 and dy==0, "PLAYER IDLE/other (dx==0,dy==0)")

    # ENEMY: bank 2
    cycles_for_state(long, {2}, lambda dx,dy: abs(dx)>=1 or dy!=0, "ENEMY MOVING (bank2)")
    cycles_for_state(long, {2}, lambda dx,dy: dx==0 and dy==0, "ENEMY STILL/death (bank2)")

    # bank 9 (possible 2nd enemy / item)
    cycles_for_state(long, {9}, lambda dx,dy: True, "BANK9 all")

if __name__=="__main__":
    main()
