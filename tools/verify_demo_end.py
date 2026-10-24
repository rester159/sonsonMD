#!/usr/bin/env python3
"""Original-byte-backed demo all-out countdown for every clock phase."""
from pathlib import Path
import hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/demo-end';o.mkdir(parents=True,exist_ok=True)
rom=b''.join((r/'arcade'/n).read_bytes() for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(rom).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
for address,data in [(0x97c6,'bd6452bd6466c604d70a0c0b'),(0x97d2,'d624c43f260a0a0a26060c080f0b0f0e39'),(0x97e3,'0c055fd708d70bd70e39')]:
 b=bytes.fromhex(data);assert rom[address-0x4000:address-0x4000+len(b)]==b
c='''#include <assert.h>
#include "arcade_demo_end.h"
int main(void){
 for(unsigned clock=0;clock<256;clock++){
  ArcadeDemoEnd s={1,0};unsigned boundaries=0;
  for(unsigned n=0;n<260;n++){
   unsigned now=(clock+n)&255;
   u8 flags=arcade_demo_end_step(&s,now);
   if(boundaries==4){assert(flags==ARCADE_DEMO_END_DONE);assert((now&63)==1);break;}
   assert(flags==(n==0?ARCADE_DEMO_END_SOUND:0));
   if(!(now&63))boundaries++;
   assert(s.timer==4-boundaries);assert(s.phase==(boundaries==4?3:2));
  }
  assert(boundaries==4);
 }
 return 0;
}
'''
(o/'test.c').write_text(c)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),str(r/'src/arcade_demo_end.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'passed':True,'clock_phases':256,'scope':'Source97C6 setup falls through to97D2; four phased decrements, next-tick97E3 exit. Byte signatures plus independent boundary-count oracle under ASan/UBSan. No sound-driver or rendering proof.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
