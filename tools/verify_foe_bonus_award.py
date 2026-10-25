#!/usr/bin/env python3
"""Native award transaction versus original contact plus actual score consumer."""
from pathlib import Path
import ctypes as C,subprocess,csv,json,hashlib
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/foe-bonus-award';o.mkdir(exist_ok=True)
(o/'wrapper.c').write_text('''#include "arcade_rewards.h"
void probe(unsigned char kind,unsigned char owner,unsigned short combat,unsigned short *v){
 ArcadeFoe a={0};ArcadeBonusAward out={0};a.body.active=65;a.body.state=255;a.body.phase=1;a.kind=kind;a.hit_style=1-owner;
 v[7]=arcade_gh_bonus_collect(&a,owner,combat,&out);
 v[0]=a.body.active;v[1]=a.body.phase;v[2]=a.hit_style;v[3]=out.event;v[4]=out.sound;v[5]=out.combat;v[6]=out.points;
}
''')
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(o/'wrapper.c'),str(r/'src/arcade_rewards.c'),str(r/'src/arcade_difficulty.c'),'-o',str(o/'test.dylib')],check=True)
lib=C.CDLL(str(o/'test.dylib'));lib.probe.argtypes=[C.c_uint8,C.c_uint8,C.c_uint16,C.POINTER(C.c_uint16)]
trace=r/'out/parity-audit/foe-bonus-collect.csv';rows=list(csv.DictReader(trace.open()));assert len(rows)==176
fields=['active','phase','credit','event','sound','after_combat','points']
for row in rows:
 v=(C.c_uint16*8)();lib.probe(int(row['index']),int(row['owner']),int(row['combat']),v)
 assert v[7]==1 and list(v)[:7]==[int(row[k])for k in fields],row
report={'cases':len(rows),'field_checks':len(rows)*7,'trace_sha256':hashlib.sha256(trace.read_bytes()).hexdigest(),'source_sha256':hashlib.sha256((r/'src/arcade_rewards.c').read_bytes()).hexdigest(),'scope':'Host compiled successful converted G/H collection transaction versus original contact and BCD consumer. Native contact/bonus lifecycle and artwork integration pending.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
