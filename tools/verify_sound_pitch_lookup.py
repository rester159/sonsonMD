#!/usr/bin/env python3
"""Compare actual old/new YM mapping routines at the ordered write boundary."""
from pathlib import Path
import subprocess
import json
root=Path(__file__).resolve().parents[1]
out=root/'out/parity-audit/sound-pitch-lookup'
out.mkdir(parents=True,exist_ok=True)
old=(root/'tools/fixtures/sound_pitch_before_lookup.c').read_text()
source=(root/'src/sound.c').read_text()
a=source.index('#define SFX_TL_BIAS');b=source.index('/* ===========================================================================',a)
c='''#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "sound_ym_pitch.h"
static u8 ay[3][16],ym_keyed[3],ym_keyed2[3];
static u16 writes[32][3];static unsigned count;
static void YM2612_writeReg(u16 bank,u16 reg,u16 value){assert(count<32);writes[count][0]=bank;writes[count][1]=reg;writes[count++][2]=value;}
'''
for name,body in [('before',old),('after',source[a:b])]:
    c+=body.replace('ay1_to_ym(',name+'_sfx(').replace('ay_1up_to_ym(',name+'_oneup(')
c+='''static void compare(u8 keys){u16 expected[32][3];u8 key1[3],key2[3];unsigned n,k;
 for(k=0;k<3;k++){ym_keyed[k]=(keys>>k)&1;ym_keyed2[k]=(keys>>(2-k))&1;}
 memset(writes,0,sizeof writes);count=0;before_sfx();before_oneup();n=count;
 memcpy(expected,writes,sizeof writes);memcpy(key1,ym_keyed,3);memcpy(key2,ym_keyed2,3);
 for(k=0;k<3;k++){ym_keyed[k]=(keys>>k)&1;ym_keyed2[k]=(keys>>(2-k))&1;}
 memset(writes,0,sizeof writes);count=0;after_sfx();after_oneup();
 assert(n==count&&!memcmp(expected,writes,sizeof writes));
 assert(!memcmp(key1,ym_keyed,3)&&!memcmp(key2,ym_keyed2,3));
}
int main(void){unsigned period,volume,keys,mixer,bank,k,trial;u32 seed=71;
 for(period=1;period<4096;period++){
  u32 total=1845281UL/period;u8 block=0;
  while(total>=2048&&block<7){total>>=1;block++;}if(total>=2048)total=2047;
  assert(sound_ym_pitch[period]==((block<<11)|total));
 }
 for(period=0;period<4096;period++)for(volume=0;volume<32;volume++)for(keys=0;keys<8;keys++)for(mixer=0;mixer<2;mixer++){
  memset(ay,0,sizeof ay);
  for(bank=0;bank<3;bank+=2){ay[bank][7]=mixer?255:0;
   for(k=0;k<3;k++){u16 p=(period+k*273)&4095;ay[bank][k*2]=(u8)p;ay[bank][k*2+1]=(p>>8)|0xf0;ay[bank][8+k]=(volume+k)&31;}}
  compare(keys);
 }
 for(trial=0;trial<200000;trial++){
  for(bank=0;bank<3;bank++)for(k=0;k<16;k++){seed=seed*1664525u+1013904223u;ay[bank][k]=seed>>24;}
  compare(trial&7);
 }return 0;}
'''
(out/'test.c').write_text(c)
subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'inc'),str(out/'test.c'),str(root/'src/sound_ym_pitch.c'),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True)
report={'nonzero_periods':4095,'register_sequence_cases':2297152,'scope':'Actual old/new SFX and 1-up mapping blocks; ordered bank/address/value writes and final key state, all periods, volume/envelope values, key combinations and mixed register patterns. Hardware bus timing, audible equivalence and source soundtrack fidelity are not proved.'}
(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))
