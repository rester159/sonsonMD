#!/usr/bin/env python3
"""Original byte guards, full parameter countdowns and independent formation bytes."""
from pathlib import Path
import hashlib,subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/c-wave';o.mkdir(parents=True,exist_ok=True)
b=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(b).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
assert b[0xf27c-0x4000:0xf280-0x4000]==bytes.fromhex('96 a7 26 2b')
assert b[0xf28a-0x4000:0xf2ab-0x4000]==bytes.fromhex('e6 0e e7 01 c6 1e e7 0f 6c 03 6a 0f 26 13 c6 1e e7 0f 6a 01 26 0b c6 06 d7 b0 5f e7 07 6c 02 6f 03')
assert b[0xf2c6-0x4000:0xf2d6-0x4000]==bytes.fromhex('d6 24 54 54 54 54 54 d8 96 c4 07 86 0c 3d 31 ab')
t=b[0xf2f8-0x4000:0xf358-0x4000]
c=r'''
#include <assert.h>
#include <string.h>
#include "arcade_c_wave.h"
static const u8 original[96]={__TABLE__};
int main(void){unsigned parameter,tick,clock,difficulty,cam,k;
 for(parameter=0;parameter<256;parameter++){
  ArcadeCWave w={0};ArcadeCBirth birth[6],poison[6];unsigned ticks=30*(parameter?parameter:256);
  w.control.active=1;w.control.parameter=parameter;w.kind=77;w.reservation=91;
  memset(birth,0xa5,sizeof(birth));memcpy(poison,birth,sizeof(birth));
  for(tick=0;tick<ticks;tick++){
   ArcadeCWave held=w;
   /* Live formation freezes all countdown state, including entry phase. */
   assert(!arcade_c_wave_step(&w,1,tick,0,0,birth));assert(!memcmp(&w,&held,sizeof(w)));
   assert(!arcade_c_wave_step(&w,0,tick,0,0,birth));assert(!memcmp(birth,poison,sizeof(birth)));
   assert(w.timer==30-(tick+1)%30);
   if(tick+1<ticks){assert(!w.control.state && w.control.phase==1);assert(w.control.wait==(u8)(parameter-(tick+1)/30));assert(w.kind==77 && w.reservation==91);}
  }
  assert(w.control.state==1 && !w.control.phase && !w.control.wait && !w.kind && w.reservation==6);
  assert(arcade_c_wave_step(&w,255,0,0,65530,birth)==6); /* State1 does not recheck live. */
  assert(!w.control.state && !w.control.phase);
  for(k=0;k<6;k++)assert(birth[k].x==242 && birth[k].y==original[k*2] && birth[k].delay==original[k*2+1] && !birth[k].kind);
 }
 for(clock=0;clock<256;clock++)for(difficulty=0;difficulty<256;difficulty++)for(cam=0;cam<3;cam++){
  ArcadeCWave w={0};ArcadeCBirth birth[6];unsigned seq=((clock>>5)^difficulty)&7;u16 camera=cam==0?0:cam==1?32767:65535;
  w.control.active=1;w.control.state=1;w.kind=23;w.reservation=6;
  assert(arcade_c_wave_step(&w,6,clock,difficulty,camera,birth)==6);
  for(k=0;k<6;k++)assert(birth[k].x==(u16)(camera+248) && birth[k].y==original[seq*12+k*2] && birth[k].delay==original[seq*12+k*2+1] && birth[k].kind==23);
  assert(w.reservation==6 && w.kind==23);
 }
 for(k=0;k<256;k++){
  ArcadeCWave w,old;ArcadeCBirth birth[6],old_birth[6];memset(&w,k,sizeof(w));w.control.active=0;old=w;
  memset(birth,0xa5,sizeof(birth));memcpy(old_birth,birth,sizeof(birth));assert(!arcade_c_wave_step(&w,0,0,0,0,birth));
  assert(!memcmp(&w,&old,sizeof(w)) && !memcmp(birth,old_birth,sizeof(birth)));
 }
 return 0;}
'''.replace('__TABLE__',','.join(map(str,t)))
(o/'test.c').write_text(c)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),str(r/'src/arcade_c_wave.c'),str(r/'src/arcade_c_wave_data.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'parameter_countdowns':256,'countdown_ticks':30*sum(range(1,257)),'formation_selections':256*256*3,'inactive_retention':256,'scope':'Offline F269 contract and ROM instruction/table guards: live freeze, entry fallthrough, byte countdown wrap, reservation timing and six-slot births. Not original execution trace or live C-family binding.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
