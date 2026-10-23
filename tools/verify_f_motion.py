#!/usr/bin/env python3
"""F source-table and phase arithmetic contracts; offline host validation."""
from pathlib import Path
import hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/f-motion';o.mkdir(parents=True,exist_ok=True)
b=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(b).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
def raw(a,n):return b[a-0x4000:a-0x4000+n]
def guard(a,h):assert raw(a,len(bytes.fromhex(h)))==bytes.fromhex(h)
guard(0xdff0,'ce e0 30 ec 0a e3 c4 ed 0a 4d 2a 0a 84 7f a7 0a e6 09 c8 01 e7 09')
guard(0xe030,'02 8d')
guard(0xe020,'a6 08 e6 09 26 05 9b ca a7 18 39 90 ca a7 18 39')
# E162 byte operations, following its temporary stack layout and carry chain.
def source_product_hi(x,y):
 m=[0]*8;m[4:8]=[x>>8,x&255,y>>8,y&255]
 a,b=m[4],m[6];m[0:2]=[a,b];m[3]=b
 b,a=m[5],m[7];m[5:7]=[a,b];m[2]=b
 def mul(j):
  v=m[j]*m[j+1];return v>>8,v&255
 a,b=mul(6);m[6:8]=[a,b]
 a,b=mul(4);v=b+m[6];b=v&255;a=(a+(v>>8))&255;m[5:7]=[a,b]
 a,b=mul(2);v=b+m[6];b=v&255;v=a+m[5]+(v>>8);a=v&255;m[5:7]=[a,b];m[4]=(v>>8)&1
 a,b=mul(0);v=b+m[5];b=v&255;a=(a+m[4]+(v>>8))&255
 return (a<<8)|b
for amplitude in range(256):
 for sample in range(256):
  assert source_product_hi(amplitude<<8,sample)==(amplitude*sample)//256
table=raw(0xe0e2,128)
vectors=[(int.from_bytes(raw(0xe09a+i*5,3),'big',signed=True),int.from_bytes(raw(0xe09d+i*5,2),'big',signed=True))for i in range(9)]
cruise=[int.from_bytes(raw(0xe0c7+i*3,3),'big',signed=True)for i in range(9)]
c=r'''
#include <assert.h>
#include "arcade_f_motion.h"
static const u8 source[128]={__TABLE__};
static const ArcadeFVector entry[9]={__ENTRY__};
static const s32 cruise[9]={__CRUISE__};
int main(void){unsigned phase,half,amplitude,origin,i;
for(i=0;i<9;i++){assert(arcade_f_entry_vectors[i].dx==entry[i].dx&&arcade_f_entry_vectors[i].dy==entry[i].dy);assert(arcade_f_cruise_vectors[i]==cruise[i]);}
for(i=0;i<128;i++)assert(arcade_f_sine[i]==source[i]);
for(phase=0;phase<65536;phase++)for(half=0;half<2;half++)for(amplitude=0;amplitude<4;amplitude++){
 const unsigned amplitudes[4]={0,1,48,255};u16 p=phase;u8 h=half;
 unsigned lo=(phase&255)+0x8d,hi=((phase>>8)+2+(lo>>8))&255;
 unsigned rh=half^(hi>>7),index=hi&127,product=0,j;
 /* Repeated byte accumulation models the high product byte independently. */
 for(j=0;j<amplitudes[amplitude];j++)product+=source[index];
 origin=(phase*17+half*255)&255;
 unsigned expected=(rh?origin-product/256:origin+product/256)&255;
 assert(arcade_f_wave_y(&p,&h,origin,amplitudes[amplitude])==expected);
 assert(p==((index<<8)|(lo&255))&&h==rh);
}
/* Full amplitude/sample domain through a phase that selects each source entry. */
for(i=0;i<128;i++)for(amplitude=0;amplitude<256;amplitude++)for(origin=0;origin<256;origin+=255){
 u16 p=(u16)((i<<8)-0x28d);u8 h=(u8)(((u16)(p+0x28d))>>15);
 assert(arcade_f_wave_y(&p,&h,origin,amplitude)==(u8)(origin+(amplitude*source[i])/256));
}
return 0;}
'''.replace('__TABLE__',','.join(map(str,table))).replace('__ENTRY__',','.join('{%d,%d}'%v for v in vectors)).replace('__CRUISE__',','.join(map(str,cruise)))
(o/'test.c').write_text(c)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),str(r/'src/arcade_f_motion.c'),str(r/'src/arcade_f_motion_data.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'phase_cases':65536*2*4,'amplitude_cases':128*256*2,'raw_vectors':18,'source_product_cases':65536,'scope':'Pinned-ROM phase/branch/table contracts; wrapped arithmetic and source-table byte fidelity under sanitizers. E162 byte-operation reference verifies the specialized product domain; original CPU execution, F actor integration and runtime reachability remain pending.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
