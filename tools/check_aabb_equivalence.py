#!/usr/bin/env python3
"""Exercise the actual entity helper against the former four-boundary definition."""
from pathlib import Path
import hashlib,json,re,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/aabb-host';o.mkdir(parents=True,exist_ok=True)
src=(r/'src/entity.c').read_text();helper=re.search(r'static inline u8 aabb\([^}]+\}',src).group()
s='''#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
typedef int16_t s16;typedef int32_t s32;typedef uint32_t u32;typedef uint8_t u8;
'''+helper+'''
static int original(s16 ax,s16 ay,s16 aw,s16 ah,s16 bx,s16 by,s16 bw,s16 bh){return ax<bx+bw&&ax+aw>bx&&ay<by+bh&&ay+ah>by;}
int main(void){unsigned checks=0;int sizes[]={6,8,16};
 for(unsigned i=0;i<3;i++)for(unsigned j=0;j<3;j++)for(int delta=-65535;delta<=65535;delta++){
  s16 b=delta>=0?-32768:32767,a=(s16)(b+delta);
  if(aabb(a,0,sizes[i],16,b,0,sizes[j],16)!=original(a,0,sizes[i],16,b,0,sizes[j],16))return 1;
  if(aabb(0,a,16,sizes[i],0,b,16,sizes[j])!=original(0,a,16,sizes[i],0,b,16,sizes[j]))return 2;
  checks+=2;
 }
 uint32_t rng=1;
 for(unsigned i=0;i<1000000;i++){
  s16 v[4];for(unsigned k=0;k<4;k++){rng=rng*1664525u+1013904223u;v[k]=(s16)(rng>>16);}
  int a=aabb(v[0],v[1],8,6,v[2],v[3],16,16),b=original(v[0],v[1],8,6,v[2],v[3],16,16);if(a!=b)return 3;checks++;
 }
 printf("%u\\n",checks);return 0;
}
'''
(o/'check.c').write_text(s);subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=undefined,address',str(o/'check.c'),'-o',str(o/'check')],check=True)
checks=int(subprocess.check_output([str(o/'check')],text=True));result={'checks':checks,'differences':0,'helper_sha256':hashlib.sha256(helper.encode()).hexdigest(),'scope':'Every feasible s16 axis difference for positive sizes6/8/16 on both axes, plus1000000 full rectangle cases; former native overlap semantics, not arcade collision parity.'}
(o/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
