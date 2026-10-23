#!/usr/bin/env python3
"""E controller clock-phase/countdown and ROM formation contracts."""
from pathlib import Path
import hashlib,subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/e-wave';o.mkdir(parents=True,exist_ok=True)
b=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(b).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
def guard(a,h):assert b[a-0x4000:a-0x4000+len(bytes.fromhex(h))]==bytes.fromhex(h)
guard(0xf456,'96 a9 26 1e')
guard(0xf464,'e6 0e e7 01 6c 03 96 24 84 1f 26 08 6a 01 26 04 6c 02 6f 03 39')
guard(0xf481,'10 8e f4 bc d6 96 c4 38 54 31 a5')
guard(0xf4a4,'c6 09 e7 5a c6 80 e7 50 4f 5f ed 51')
t=b[0xf4bc-0x4000:0xf4dc-0x4000]
c=r'''
#include <assert.h>
#include <string.h>
#include "arcade_e_wave.h"
static const u8 original[32]={__TABLE__};
int main(void){unsigned parameter,start,tick,k,clock,diff,cam;unsigned long ticks=0;
for(parameter=0;parameter<256;parameter++)for(start=0;start<32;start++){
 ArcadeWaveControl c={0};ArcadeEBirth birth[2],poison[2];
 unsigned length=((32-start)&31)+32*((parameter?parameter:256)-1)+1,counts=0;
 c.active=1;c.parameter=parameter;c.auxiliary=0xdead;
 memset(birth,0xa5,sizeof birth);memcpy(poison,birth,sizeof birth);
 for(tick=0;tick<length;tick++){
  ArcadeWaveControl held=c;
  assert(!arcade_e_wave_step(&c,255,(u8)(start+tick),0,0,birth));assert(!memcmp(&c,&held,sizeof c));
  assert(!arcade_e_wave_step(&c,0,(u8)(start+tick),0,0,birth));assert(!memcmp(birth,poison,sizeof birth));
  if(!((start+tick)&31))counts++;
  assert(c.wait==(u8)(parameter-counts)&&c.auxiliary==0xdead);
  assert(c.state==(tick+1==length)&&c.phase==(tick+1==length?0:1));ticks++;
 }
 assert(arcade_e_wave_step(&c,255,0,0,65530,birth)==2);
 assert(!c.state&&!c.phase&&c.active==1&&c.auxiliary==0xdead);
 for(k=0;k<2;k++)assert(birth[k].x==242&&birth[k].y==original[k*2]&&birth[k].delay==original[k*2+1]&&birth[k].direction==9);
}
for(clock=0;clock<256;clock++)for(diff=0;diff<256;diff++)for(cam=0;cam<3;cam++){
 ArcadeWaveControl c={0};ArcadeEBirth birth[2];u16 camera=cam==0?0:cam==1?32767:65535;
 unsigned group=(diff&56)>>3;c.active=1;c.state=1;
 assert(arcade_e_wave_step(&c,255,clock,diff,camera,birth)==2);
 for(k=0;k<2;k++)assert(birth[k].x==(u16)(camera+248)&&birth[k].y==original[group*4+k*2]&&birth[k].delay==original[group*4+k*2+1]&&birth[k].direction==9);
}
for(k=0;k<256;k++){
 ArcadeWaveControl c,old;ArcadeEBirth birth[2],old_birth[2];memset(&c,k,sizeof c);c.active=0;old=c;
 memset(birth,k,sizeof birth);memcpy(old_birth,birth,sizeof birth);
 assert(!arcade_e_wave_step(&c,0,0,0,0,birth));assert(!memcmp(&c,&old,sizeof c)&&!memcmp(birth,old_birth,sizeof birth));
}
assert(ticks==33558528UL);
return 0;}
'''.replace('__TABLE__',','.join(map(str,t)))
(o/'test.c').write_text(c)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),str(r/'src/arcade_e_wave.c'),str(r/'src/arcade_e_wave_data.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'countdown_sequences':8192,'formation_selections':196608,'scope':'Source byte/table guards and sanitized host contracts: every parameter/clock phase, live freeze, byte wrap, next-tick two-slot births and difficulty-only formation selection. No original CPU execution or live E binding.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
