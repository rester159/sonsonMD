#!/usr/bin/env python3
"""Compare native result-map output with source character pixels and placement."""
from pathlib import Path
import ctypes as C
import json,subprocess
import gen_assets as G
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/results-map'
o.mkdir(parents=True,exist_ok=True)
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_results_map.c'),str(r/'src/arcade_results_art.c'),'-o',str(o/'test.dylib')],check=True)
lib=C.CDLL(str(o/'test.dylib'))
class Event(C.Structure):
 _fields_=[('points',C.c_uint32*2),('flags',C.c_uint16)]+[(n,C.c_uint8)for n in ('players','marker','number')]
lib.arcade_results_map_clear.argtypes=[C.POINTER(C.c_uint16)]
lib.arcade_results_map_event.argtypes=[C.POINTER(C.c_uint16),C.c_uint8,C.POINTER(Event)]
import re
count=int(re.search(r'#define ARCADE_RESULTS_ART_TILES (\d+)',(r/'inc/arcade_results_art.h').read_text())[1])
pixels=list((C.c_uint32*(count*8)).in_dll(lib,'arcade_results_pixels'))
tiles=[tuple(pixels[i:i+8])for i in range(0,len(pixels),8)]
g=G.Roms(str(r/'arcade'))
def bake(code,attr=0x2c):return tuple(G.bake_char_tile(g,code+((attr&3)<<8),attr>>2))
ink={c:bake(ord(c)-0x30)for c in 'CONGRATULATIONBPSDM@0123456789'}
clear=bake(16,0);blank=bake(16)
checks=0
for number in list(range(10))+[16]:
 for players in range(4):
  for marker in range(3):
   m=(C.c_uint16*1024)(*[65535]*1024)
   expected=[None]*1024
   e=Event();e.flags=2;lib.arcade_results_map_event(m,number,C.byref(e))
   expected[160:992]=[clear]*832
   def label(cell,text):
    for i,c in enumerate(text):expected[cell+i]=ink[c]
   e.flags=4;lib.arcade_results_map_event(m,number,C.byref(e))
   label(0xe9,'CONGRATULATION')
   e.flags=8;e.players=players;e.marker=marker
   lib.arcade_results_map_event(m,number,C.byref(e))
   points=(10 if number==16 else number)*10000
   digits=str(points).rjust(8,'@')
   for p in range(2):
    offset=p*256
    if players&(1<<p):
     label(0x168+offset,'BONUS');label(0x176+offset,'PTS')
     label(0x16d+offset,digits)
    if marker==p+1:
     label(0x1ae+offset,'AND');label(0x1f0+offset,'30000@PTS')
   for i,value in enumerate(expected):
    if value is None:assert m[i]==65535,(number,players,marker,i)
    else:assert m[i]<count and tiles[m[i]]==value,(number,players,marker,i)
    checks+=1
report={'map_cells_checked':checks,'cases':132,'tile_count':count,
 'scope':'Host pixel-level comparison of production results map composer: clear range/HUD preservation, source label positions, all valid displayed course values, player masks and markers. Live VDP scene integration pending.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
