#!/usr/bin/env python3
"""Source dispatch-table steering, exact comparisons, and movement-vector bytes."""
from pathlib import Path
import hashlib,subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/e-motion';o.mkdir(parents=True,exist_ok=True)
b=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(b).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
def word(a):return int.from_bytes(b[a-0x4000:a-0x4000+2],'big')
handlers=[word(0xc45b+2*i)for i in range(17)]
assert set(handlers)=={0xc4b2,0xc48f,0xc486,0xc497,0xc47d}
assert b[0xc49f-0x4000:0xc4b5-0x4000]==bytes.fromhex('e6 1a c1 10 26 01 5f 5c 39 e6 1a c1 01 26 02 c6 11 5a 39 c6 01 39')
raw=b''.join(b[word(0xc5ee+i*2)-0x4000:word(0xc5ee+i*2)-0x4000+85]for i in range(2))
c=r'''
#include <assert.h>
#include "arcade_e_motion.h"
static const unsigned handlers[17]={__HANDLERS__};
static const u8 raw[170]={__RAW__};
static unsigned reference(unsigned d,unsigned x,unsigned y,unsigned tx,unsigned ty){
 unsigned dest;
 switch(handlers[d]){
 case 0xc4b2:return 1;
 case 0xc48f:dest=y<ty?0xc4a8:0xc49f;break;
 case 0xc486:dest=x<tx?0xc4a8:0xc49f;break;
 case 0xc497:dest=y<ty?0xc49f:0xc4a8;break;
 default:dest=x<tx?0xc49f:0xc4a8;break;
 }
 if(dest==0xc49f){if(d==16)d=0;return d+1;}
 if(d==1)d=17;return d-1;
}
int main(void){unsigned d,x,y,t,k,i;const u16 xs[]={0,32767,32768,65535};
 for(d=0;d<17;d++)for(x=0;x<65536;x++)for(t=0;t<4;t++)
  assert(arcade_e_turn(d,x,(u8)x,xs[t],127)==reference(d,x,(u8)x,xs[t],127));
 for(d=0;d<17;d++)for(y=0;y<256;y++)for(t=0;t<256;t++)
  assert(arcade_e_turn(d,32768,y,32767,t)==reference(d,32768,y,32767,t));
 for(i=0;i<2;i++)for(k=0;k<17;k++){
  unsigned a=(i*17+k)*5;int dx=raw[a]*65536+raw[a+1]*256+raw[a+2],dy=raw[a+3]*256+raw[a+4];
  if(dx&0x800000)dx-=0x1000000;
  if(dy&32768)dy-=65536;
  assert(arcade_e_vectors[i][k].dx==dx&&arcade_e_vectors[i][k].dy==dy);
 }
 return 0;}
'''.replace('__HANDLERS__',','.join(map(str,handlers))).replace('__RAW__',','.join(map(str,raw))).replace('if(d==1)d=17;return d-1;','if(d==1)d=17;\n return d-1;')
(o/'test.c').write_text(c)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),str(r/'src/arcade_e_motion.c'),str(r/'src/arcade_e_motion_data.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'steering_cases':17*65536*4+17*256*256,'vectors':34,'scope':'Offline source dispatch-reference cases, unsigned boundaries/ties/direction wrap and ROM vector comparison. No live E integration or original CPU execution.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
