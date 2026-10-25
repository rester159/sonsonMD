#!/usr/bin/env python3
"""D steering versus branch/bit-division reference, and raw source vectors."""
from pathlib import Path
import hashlib,subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/d-motion';o.mkdir(parents=True,exist_ok=True)
b=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(b).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
def guard(a,h):assert b[a-0x4000:a-0x4000+len(bytes.fromhex(h))]==bytes.fromhex(h)
guard(0xc0db,'0f cc 10 ae 0e ec 35 a3 15 24 09 43 53 c3 00 01 0c cc 08 cc a6 38 a0 18 24 03 40 0c cc dd ca bd c2 9c')
guard(0xc29c,'34 04 c6 08 34 04 5f 48 59 4c e2 61 24 03 4a eb 61 6a e4 26 f2 32 62 39')
guard(0xc0ff,'c6 04 81 05 22 0e 5a 81 02 24 09 5a 96 ca 44 91 cd 24 01 5a d7 ce')
c=r'''
#include <assert.h>
#include "arcade_d_motion.h"
static unsigned reference(unsigned x,unsigned y,unsigned tx,unsigned ty){
 unsigned quadrant=0,d=(tx-x)&65535,a,den,rem=0,k,angle;
 if(tx<x){d=((~d)+1)&65535;quadrant++;quadrant<<=1;}
 a=(ty-y)&255;if(ty<y){a=(-a)&255;quadrant++;}
 unsigned saved=a;den=d&255;
 /* C29C unsigned long division, eight shifts/subtracts; includes divisor0. */
 for(k=0;k<8;k++){
  unsigned carry=a>>7;a=(a<<1)&255;rem=(rem<<1)|carry;a++;
  if(rem<den){a=(a-1)&255;}else rem-=den;
 }
 angle=4;if(a<=5){angle--;if(a<2){angle--;if((saved>>1)<rem)angle--;}}
 switch(quadrant){case 0:return 17-angle;case 1:return 1+angle;case 2:return 9+angle;default:return 9-angle;}
}
static const unsigned char raw[170]={__VECTORS__};
int main(void){unsigned dx,dy,q,i,j,y,ty;const u16 xs[]={0,1,127,128,255,256,32767,32768,65535};
 for(dx=0;dx<256;dx++)for(dy=0;dy<256;dy++)for(q=0;q<4;q++){
  u16 x=q&2?dx:0,tx=q&2?0:dx;u8 y0=q&1?dy:0,y1=q&1?0:dy;
  assert(arcade_d_aim(x,y0,tx,y1)==reference(x,y0,tx,y1));
 }
 for(i=0;i<9;i++)for(j=0;j<9;j++)for(y=0;y<256;y++)for(ty=0;ty<256;ty++)
  assert(arcade_d_aim(xs[i],y,xs[j],ty)==reference(xs[i],y,xs[j],ty));
 for(i=0;i<2;i++)for(j=0;j<17;j++){
  unsigned a=(i*17+j)*5;int x=raw[a]*65536+raw[a+1]*256+raw[a+2],y1=raw[a+3]*256+raw[a+4];
  if(x&0x800000)x-=0x1000000;if(y1&32768)y1-=65536;
  assert(arcade_d_vectors[i][j].dx==x&&arcade_d_vectors[i][j].dy==y1);
 }
 return 0;}
'''.replace('__VECTORS__',','.join(map(str,b[0xc1f2-0x4000:0xc29c-0x4000])))
c=c.replace('if(x&0x800000)x-=0x1000000;if(y1&32768)y1-=65536;','if(x&0x800000)x-=0x1000000;\n  if(y1&32768)y1-=65536;')
(o/'test.c').write_text(c)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),str(r/'src/arcade_d_motion.c'),str(r/'src/arcade_d_motion_data.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'steering_cases':256*256*4+9*9*256*256,'vectors':34,'scope':'ROM-guarded offline steering reference including zero division and unsigned world-coordinate comparisons. Native vector data compared with raw ROM. No source CPU execution or live D binding.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
