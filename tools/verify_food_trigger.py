#!/usr/bin/env python3
"""Compare native E607 transaction against original CPU fixtures."""
from pathlib import Path
import ctypes as C,csv,hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/food-trigger';o.mkdir(exist_ok=True)
(o/'wrapper.c').write_text('#include "arcade_food.h"\nvoid probe(unsigned char count,unsigned char total,unsigned char food,unsigned char grade,unsigned char *v){\n ArcadeFoodCounter c={count,total};ArcadeSpecialSpawn s={77,66,55,44};ArcadePowerup p={0};\n p.kind=99;p.exit_latched=88;\n v[8]=arcade_food_collect(&c,food,0x68,grade,&s,&p);\n v[0]=c.count;v[1]=c.total;v[2]=p.kind;v[3]=p.exit_latched;\n v[4]=s.active;v[5]=s.phase;v[6]=s.placement;v[7]=s.row;\n}\n')
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(o/'wrapper.c'),str(r/'src/arcade_food.c'),'-o',str(o/'test.dylib')],check=True)
lib=C.CDLL(str(o/'test.dylib'));lib.probe.argtypes=[C.c_uint8]*4+[C.POINTER(C.c_uint8)]
trace=r/'out/parity-audit/food-trigger.csv';rows=list(csv.DictReader(trace.open()));assert len(rows)==28200
fields=['after_count','after_total','kind','latch','active','phase','placement','row']
for row in rows:
 v=(C.c_uint8*9)();lib.probe(*[int(row[k])for k in ['count','total','food','grade']],v)
 assert list(v)[:8]==[int(row[k])for k in fields],(row,list(v))
 assert v[8]==(int(row['count'])==5)
report={'cases':len(rows),'field_checks':len(rows)*8,'trace_sha256':hashlib.sha256(trace.read_bytes()).hexdigest(),'source_sha256':hashlib.sha256((r/'src/arcade_food.c').read_bytes()).hexdigest(),'scope':'Host six-food aggregate and spawn descriptor against original CPU; signed food codes F4..07, reachable sums, grades0..8/255. Full food tile contact and live binding pending.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
