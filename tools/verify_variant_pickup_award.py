#!/usr/bin/env python3
"""Native award transaction versus original contact plus actual score consumer."""
from pathlib import Path
import ctypes as C,subprocess,csv,json,hashlib
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/variant-pickup-award';o.mkdir(exist_ok=True)
(o/'wrapper.c').write_text('''#include "arcade_rewards.h"
void probe(unsigned char count,unsigned char kind,unsigned char owner,unsigned short combat,unsigned short *v){
 ArcadePowerup a={0},conversion={0};ArcadeSpecialSpawn spawn={0};ArcadeBonusAward out={0};
 a.active=1;a.state=2;a.kind=10;a.owner=1-owner;a.x=100;a.y=136;
 v[12]=arcade_variant_pickup_collect(&a,kind,100,136,owner,combat,&count,&spawn,&conversion,&out);
 v[0]=a.active;v[1]=a.phase;v[2]=a.owner;v[3]=out.event;v[4]=out.sound;v[5]=out.combat;v[6]=out.points;
 v[7]=count;v[8]=spawn.active;v[9]=spawn.phase;v[10]=spawn.placement;v[11]=conversion.kind;
 v[13]=a.state;
}
''')
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(o/'wrapper.c'),str(r/'src/arcade_rewards.c'),str(r/'src/arcade_difficulty.c'),'-o',str(o/'test.dylib')],check=True)
lib=C.CDLL(str(o/'test.dylib'));lib.probe.argtypes=[C.c_uint8,C.c_uint8,C.c_uint8,C.c_uint16,C.POINTER(C.c_uint16)]
trace=r/'out/parity-audit/variant-pickup-collect.csv';rows=list(csv.DictReader(trace.open()));assert len(rows)==384
fields=['active','phase','credit','event','sound','after_combat','points','after_count','spawn_active','spawn_phase','spawn_placement','special_kind']
for row in rows:
 v=(C.c_uint16*14)();lib.probe(int(row['count']),int(row['index']),int(row['owner']),int(row['combat']),v)
 assert v[12]==1 and v[13]==1 and list(v)[:12]==[int(row[k])for k in fields],row
report={'cases':len(rows),'field_checks':len(rows)*12,'trace_sha256':hashlib.sha256(trace.read_bytes()).hexdigest(),'source_sha256':hashlib.sha256((r/'src/arcade_rewards.c').read_bytes()).hexdigest(),'scope':'Host kind10 variants0..7 collection, unchanged combat difficulty, score consumer and conversion-spawn counter versus original CPU. Target execution, creation and live binding pending.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
