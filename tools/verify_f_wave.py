#!/usr/bin/env python3
"""Pinned-ROM F wave countdown, reservation and formation contracts."""
from pathlib import Path
import hashlib,subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/f-wave';o.mkdir(parents=True,exist_ok=True)
b=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(b).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
def guard(a,h):
 v=bytes.fromhex(h);assert b[a-0x4000:a-0x4000+len(v)]==v
guard(0xf4ef,'96 ae 26 22 58 ce f4 f9 6e d5 f4 fd f5 03 e6 0e e7 01 6c 03 96 24 84 1f 26 0c 6a 01 26 08 c6 04 d7 b6 6c 02 6f 03 39')
guard(0xf51e,'d6 96 c4 30 54 54 54 ce f5 7a 10 ae c5 d6 96 c4 0e 58 58 31 a5 dc 90 c3 00 f8 dd c8 c6 05 d7 cb d6 96 c5 20 27 0b dc 90 83 00 08 dd c8 c6 01 d7 cb')
guard(0xf553,'ce 0b a8 ec a1 a7 58 a7 48 e7 4d dc c8 ed 55 d6 cb e7 5a c6 80 e7 50 4f 5f ed 51')
# Independently follow source pointer roots, retaining source row offset arithmetic.
t=[]
for diff in range(256):
 a=0xf57a+((diff&48)>>3);base=int.from_bytes(b[a-0x4000:a-0x4000+2],'big')+((diff&14)<<2)
 t.extend(b[base-0x4000:base-0x4000+8])
c=r'''
#include <assert.h>
#include <string.h>
#include "arcade_f_wave.h"
static const u8 original[256][8]={__TABLE__};
int main(void){unsigned param,start,tick,diff,clock,cam,k;unsigned long ticks=0;
for(param=0;param<256;param++)for(start=0;start<32;start++){
 ArcadeFWave w={0};ArcadeFBirth birth[4],poison[4];unsigned counts=0;
 unsigned duration=((32-start)&31)+32*((param?param:256)-1)+1;
 w.control.active=1;w.control.parameter=param;w.control.auxiliary=0xbeef;w.reservation=77;
 memset(birth,0xa5,sizeof birth);memcpy(poison,birth,sizeof birth);
 for(tick=0;tick<duration;tick++){
  ArcadeFWave old=w;
  assert(!arcade_f_wave_step(&w,255,(u8)(start+tick),0,0,birth));assert(!memcmp(&w,&old,sizeof w));
  assert(!arcade_f_wave_step(&w,0,(u8)(start+tick),0,0,birth));assert(!memcmp(birth,poison,sizeof birth));
  if(!((start+tick)&31))counts++;
  assert(w.control.wait==(u8)(param-counts)&&w.control.auxiliary==0xbeef);
  assert(w.control.state==(tick+1==duration)&&w.control.phase==(tick+1==duration?0:1));
  assert(w.reservation==(tick+1==duration?4:77));ticks++;
 }
 /* Birth does not recheck live and does not rewrite B6. */
 w.reservation=55;assert(arcade_f_wave_step(&w,255,1,0,65535,birth)==4);
 assert(w.reservation==55&&!w.control.state&&!w.control.phase);
}
for(diff=0;diff<256;diff++)for(clock=0;clock<256;clock++)for(cam=0;cam<3;cam++){
 ArcadeFWave w={0};ArcadeFBirth birth[4];u16 camera=cam==0?0:cam==1?32767:65535;
 w.control.active=1;w.control.state=1;w.reservation=4;
 assert(arcade_f_wave_step(&w,255,clock,diff,camera,birth)==4);
 for(k=0;k<4;k++){
  assert(birth[k].x==(u16)(camera+((diff&32)?-8:248)));
  assert(birth[k].y==original[diff][k*2]&&birth[k].origin_y==birth[k].y);
  assert(birth[k].delay==original[diff][k*2+1]&&birth[k].direction==((diff&32)?1:5));
 }
 assert(!w.control.state&&!w.control.phase&&w.reservation==4);
}
for(k=0;k<256;k++){
 ArcadeFWave w,old;ArcadeFBirth birth[4],saved[4];memset(&w,k,sizeof w);w.control.active=0;old=w;
 memset(birth,k,sizeof birth);memcpy(saved,birth,sizeof birth);
 assert(!arcade_f_wave_step(&w,0,0,0,0,birth));assert(!memcmp(&old,&w,sizeof w)&&!memcmp(saved,birth,sizeof birth));
}
assert(ticks==33558528UL);return 0;}
'''.replace('__TABLE__',','.join('{'+','.join(map(str,t[i:i+8]))+'}'for i in range(0,len(t),8)))
(o/'test.c').write_text(c)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),str(r/'src/arcade_f_wave.c'),str(r/'src/arcade_f_wave_data.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'countdown_sequences':8192,'countdown_ticks':33558528,'formation_selections':196608,'inactive_cases':256,'scope':'ROM byte and pointer-table guarded host contracts with sanitizers. F actor, sourceAE/B6 ownership and live binding pending; no original CPU runtime execution.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
