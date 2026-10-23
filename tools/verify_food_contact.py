#!/usr/bin/env python3
"""Original food contact and score consumer versus native transaction."""
from pathlib import Path
import ctypes as C,csv,hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/food-contact';o.mkdir(exist_ok=True)
(o/'wrapper.c').write_text('#include "arcade_food.h"\nvoid probe(unsigned char owner,unsigned char food,unsigned char pa,unsigned char ta,unsigned char visible,short dx,short dy,unsigned short *v){\n ArcadeFoodTile t={4,ta,3,3,food,0};ArcadeFoodAward a={0};ArcadeFoodCounter c={0};ArcadeSpecialSpawn s={0};ArcadePowerup p={0};\n if(arcade_food_contact(&t,visible?65500:500,(unsigned short)(4+dx),(unsigned char)(3+dy),pa,owner,&a))\n  arcade_food_collect(&c,food,t.y,0,&s,&p);\n v[0]=t.active;v[1]=t.phase;v[2]=c.count;v[3]=c.total;v[4]=a.event;v[5]=a.sound;v[6]=a.points;\n}\n')
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(o/'wrapper.c'),str(r/'src/arcade_food.c'),'-o',str(o/'test.dylib')],check=True)
lib=C.CDLL(str(o/'test.dylib'));lib.probe.argtypes=[C.c_uint8]*5+[C.c_int16]*2+[C.POINTER(C.c_uint16)]
trace=r/'out/parity-audit/food-contact.csv';rows=list(csv.DictReader(trace.open()));assert len(rows)==1240
fields=['after_active','phase','count','total','event','sound','points']
for row in rows:
 v=(C.c_uint16*7)();lib.probe(*[int(row[k])for k in ['owner','food','player_active','tile_active','visible','dx','dy']],v)
 assert list(v)==[int(row[k])for k in fields],(row,list(v))
report={'cases':len(rows),'field_checks':len(rows)*7,'trace_sha256':hashlib.sha256(trace.read_bytes()).hexdigest(),'source_sha256':hashlib.sha256((r/'src/arcade_food.c').read_bytes()).hexdigest(),'scope':'Host kind0 food contact and count/score event versus original CPU and BCD consumer. Both owners, active flags, camera gate, wrapped contact boundaries, all20 valid food codes. Target execution, multi-tile order, world binding pending.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
