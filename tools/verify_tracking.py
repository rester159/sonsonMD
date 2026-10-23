#!/usr/bin/env python3
"""AD53 eligibility and AD71 branch order from pinned arcade ROM."""
from pathlib import Path
import hashlib,subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/tracking';o.mkdir(parents=True,exist_ok=True)
b=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(b).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
assert b[0xad53-0x4000:0xad68-0x4000]==bytes.fromhex('10 8e 04 90 a6 33 43 a4 30 84 01 31 a8 30 e6 33 53 e4 30 c4 01')
assert b[0xad71-0x4000:0xad88-0x4000]==bytes.fromhex('d6 99 27 1c e6 63 27 18 e6 62 27 0e ec 35 10 a3 a8 25 24 03 31 a8 30')
c=r'''
#include <assert.h>
#include "arcade_tracking.h"
static unsigned reference(unsigned tracking,unsigned x0,unsigned f0,unsigned x1,unsigned f1){
 unsigned a=(~(f0&255))&255,b=(~(f1&255))&255;
 a&=f0>>8;a&=1;b&=f1>>8;b&=1;
 if(!tracking)return 0;
 if(!b)return 0;
 if(!a)return 1;
 return x0>=x1?0:1;
}
int main(void){unsigned f,g,t,i;const u16 xs[]={0,1,32767,32768,65534,65535};
 for(f=0;f<65536;f++)for(g=0;g<4;g++)for(t=0;t<2;t++)for(i=0;i<6;i++){
  unsigned other=((g&1)<<8)|(g>>1),x0=xs[i],x1=xs[(i+1)%6];
  assert(arcade_tracking_player(t,x0,f,x1,other)==reference(t,x0,f,x1,other));
  assert(arcade_tracking_player(t,x0,other,x1,f)==reference(t,x0,other,x1,f));
  assert(arcade_tracking_player(t,x0,f,x0,other)==reference(t,x0,f,x0,other));
 }
 return 0;}
''';(o/'test.c').write_text(c)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'comparisons':65536*4*2*6*3,'scope':'Source byte guards and host branch-reference checks for all active/kind bytes, both player positions, unsigned boundaries and ties. Live formation caller uses this helper; no original CPU execution claim.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
