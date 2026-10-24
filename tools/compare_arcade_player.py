#!/usr/bin/env python3
"""Compare native typed locomotion against independently captured MC6809 frames."""
import argparse,csv,ctypes as C,json,pathlib,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--trace',type=pathlib.Path,default=pathlib.Path('out/parity-audit/arcade-player.csv'));a=p.parse_args()
r=pathlib.Path(__file__).resolve().parents[1];o=r/'out/parity-audit';o.mkdir(parents=True,exist_ok=True)
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_player.c'),'-o',str(o/'arcade_player.dylib')],check=True)
class Player(C.Structure):
 _fields_=[('x',C.c_uint16)]+[(n,C.c_uint8) for n in ['xf','y','yf','state','phase','timer','stamp','facing','previous','active','attached']]
Floor=C.CFUNCTYPE(C.c_int16,C.c_uint16,C.c_uint8,C.c_void_p)
lib=C.CDLL(str(o/'arcade_player.dylib'));step=lib.arcade_player_step;step.argtypes=[C.POINTER(Player),C.POINTER(Player),C.c_uint8,C.c_uint16,Floor,C.c_void_p]
rows=list(csv.DictReader(a.trace.open()));assert len(rows)>400
current=rows[0]
@Floor
def floor(x,y,context):
 col=(((x+8-int(current['scroll']))&65535)>>3)+int(current['ring']);col&=31
 bitmap=bytes.fromhex(current['collision']);row=y>>3
 if not bitmap[col*4+(row>>3)]&(128>>(row&7)):return -32767
 low=y&15
 if low<2:return -32767
 return 7-low
q=Player(x=int(current['x']),xf=int(current['xf']),y=int(current['y']),yf=int(current['yf']),state=int(current['state']),phase=int(current['substate']),facing=int(current['face']),previous=int(current['held']),active=1)
checks=0;errors=[]
for i,row in enumerate(rows[1:],1):
 step(C.byref(q),None,int(row['held']),int(current['scroll']),floor,None)
 for actual,expected in [('x','x'),('xf','xf'),('y','y'),('yf','yf'),('state','state'),('phase','substate'),('facing','face')]:
  checks+=1
  if getattr(q,actual)!=int(row[expected]):errors.append({'tick':row['tick'],'field':actual,'native':getattr(q,actual),'arcade':int(row[expected])})
 if errors:break
 current=row
result={'trace':str(a.trace),'frames':len(rows)-1,'field_checks':checks,'first_divergences':errors,'source':'Original verified SonSon MC6809 execution in MAME 0.288; isolated locomotion, enemies removed by fixture'}
(o/'player-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));assert not errors
