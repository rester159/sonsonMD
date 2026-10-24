#!/usr/bin/env python3
"""Source-CPU trace comparison for one/two-player native locomotion."""
import argparse,csv,ctypes as C,json,pathlib,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('trace',type=pathlib.Path);a=p.parse_args()
r=pathlib.Path(__file__).resolve().parents[1];o=r/'out/parity-audit'
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_player.c'),'-o',str(o/'arcade_players.dylib')],check=True)
class Player(C.Structure):
 _fields_=[('x',C.c_uint16)]+[(n,C.c_uint8) for n in ['xf','y','yf','state','phase','timer','stamp','facing','previous','active','attached']]
Floor=C.CFUNCTYPE(C.c_int16,C.c_uint16,C.c_uint8,C.c_void_p)
lib=C.CDLL(str(o/'arcade_players.dylib'));step=lib.arcade_player_step;step.argtypes=[C.POINTER(Player),C.POINTER(Player),C.c_uint8,C.c_uint16,Floor,C.c_void_p]
sep=lib.arcade_players_separate;sep.argtypes=[C.POINTER(Player),C.POINTER(Player)]
rows=list(csv.DictReader(a.trace.open()));assert len(rows)>=1281
current=rows[0];np=2 if 'p2_x' in current else 1
@Floor
def floor(x,y,context):
 col=((((x+8-int(current['scroll']))&65535)>>3)+int(current['ring']))&31
 bitmap=bytes.fromhex(current['collision']);row=y>>3;low=y&15
 if not bitmap[col*4+(row>>3)]&(128>>(row&7)) or low<2:return -32767
 return 7-low
qs=[]
for k in range(np):
 v=lambda field:int(current[f'p{k+1}_{field}'])
 qs.append(Player(x=v('x'),xf=v('xf'),y=v('y'),yf=v('yf'),state=v('state'),phase=v('phase'),timer=v('timer'),stamp=v('stamp'),facing=v('face'),previous=v('held'),active=v('active')))
checks=0;errors=[];mount=0
for row in rows[1:]:
 for k,q in enumerate(qs):step(C.byref(q),C.byref(qs[k^1]) if np==2 else None,int(row[f'p{k+1}_held']),int(current['scroll']),floor,None)
 if np==2:sep(C.byref(qs[0]),C.byref(qs[1]))
 for k,q in enumerate(qs):
  if q.state==5:mount+=1
  for actual,expected in [('x','x'),('xf','xf'),('y','y'),('yf','yf'),('state','state'),('phase','phase'),('facing','face'),('attached','attached')]:
   checks+=1
   if getattr(q,actual)!=int(row[f'p{k+1}_{expected}']):errors.append({'tick':row['tick'],'player':k+1,'field':actual,'native':getattr(q,actual),'arcade':int(row[f'p{k+1}_{expected}'])})
 if errors:break
 current=row
result={'trace':str(a.trace),'players':np,'compared_frames':(checks//(8*np)),'field_checks':checks,'shoulder_riding_frames':mount,'first_divergences':errors,'source':'Original verified SonSon MC6809 execution, MAME 0.288, hostile records removed to isolate locomotion'}
(o/f'players-{np}p-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));assert not errors
