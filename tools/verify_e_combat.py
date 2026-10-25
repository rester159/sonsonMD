#!/usr/bin/env python3
"""E761/E78D ROM-guarded shot rules, including both-slot hits."""
from pathlib import Path
import hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/e-combat';o.mkdir(parents=True,exist_ok=True)
b=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(b).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
def guard(a,h):
 v=bytes.fromhex(h);assert b[a-0x4000:a-0x4000+len(v)]==v
guard(0xe761,'8e 0b 68 e6 30 5a 26 23 e6 38 cb 07 d7 c9 ec 35 c3 00 08 dd ca c6 02 d7 c8 e6 10 5a 26 06 e6 11 2b 02 8d 08 30 88 20 0a c8 26 ee')
guard(0xe78d,'e6 18 cb 08 d0 c9 cb 08 c1 10 22 3c ec 15 c3 00 08 93 ca c3 00 08 10 83 00 10 22 2c d6 d5 1d 84 01 a7 14 c6 02 e7 10 e7 30 cc 03 00 ed 11 cc 02 00 ed 31 34 30 cc 04 18 da d5 bd 96 04 bd 64 38 cc 00 4c bd e4 07 35 b0 39')
guard(0x6438,'86 0c 7e 63 41')
p=int.from_bytes(b[0x4db4-0x4000:0x4db6-0x4000],'big');assert int(b[p-4-0x4000:p-0x4000].hex())==300
guard(0xed5b,'ce 0b 68 c6 02 7e ec e9')
guard(0xece9,'d7 c8 e6 50 27 1b c5 02 26 17 a6 18 a0 58 8b 07 81 0e 22 0d ec 15 a3 55 c3 00 07 10 83 00 0e 23 08')
guard(0xed12,'e6 50 c5 40 27 0f c6 02 e7 50 6c 52 e6 14 e7 54 e6 53 7e e8 38')
guard(0xe89c,'09 0a 0b 0c 0d 0e 0f 10 11 12 2a')
guard(0xe838,'10 8e e8 9c e6 a5 da d5 86 04 bd 96 04 34 10 bd 64 15 cc 00 c0 bd e4 07 35 90')
guard(0x6415,'86 05 7e 63 41')
for event,points in [*[(k+9,(k+1)*1000)for k in range(10)],(0x2a,20000)]:
 a=0x4d84+event*2;p=int.from_bytes(b[a-0x4000:a-0x4000+2],'big');assert int(b[p-4-0x4000:p-0x4000].hex())==points
c=r''' 
#include <assert.h>
#include <string.h>
#include "arcade_e_combat.h"
static unsigned add(unsigned n){unsigned v=(n+76)&65535;return v<0x3f00?v:n;}
/* Independent source center/offset arithmetic, retaining byte/word wraps. */
static unsigned ref(const ArcadeEActor *a,unsigned x,unsigned y){
 unsigned cy=(y+7)&255,cx=(x+8)&65535;
 return a->active==1 && a->state<128 &&
 (((a->y+8)&255)-cy+8u)%256<=16 &&
 (((a->x+8)&65535)-cx+8u)%65536<=16;
}
int main(void){unsigned x,y,f,s,owner,combat,k;
for(x=0;x<65536;x++)for(y=0;y<256;y+=255){
 ArcadeEActor a[2]={{0}};a[0].active=a[1].active=1;
 a[0].x=65532;a[1].x=4;a[0].y=a[1].y=y;
 assert(arcade_e_shot_targets(a,x,y,1)==(ref(a,x,y)|(ref(a+1,x,y)<<1)));
}
for(y=0;y<256;y++)for(f=0;f<256;f++)for(s=0;s<256;s+=127){
 ArcadeEActor a[2]={{0}};a[0].x=65535;a[0].y=253;a[0].active=f;a[0].state=s;a[1]=a[0];
 assert(arcade_e_shot_targets(a,65535,y,1)==(ref(a,65535,y)?3:0));
 assert(!arcade_e_shot_targets(a,65535,y,0));assert(!arcade_e_shot_targets(a,65535,y,2));
}
for(owner=0;owner<2;owner++)for(combat=0;combat<65536;combat++){
 ArcadeEActor a[2]={{0}};ArcadePlayerShot shot={0};ArcadeEHit h;unsigned expected=combat;u8 mask;
 a[0].active=a[1].active=1;a[0].x=a[1].x=128;a[0].y=a[1].y=128;
 a[0].bonus_kind=a[1].bonus_kind=9;shot.active=1;mask=arcade_e_shot_targets(a,128,128,shot.active);assert(mask==3);
 for(k=0;k<2;k++)if(mask&(1<<k)){
  arcade_e_hit(a+k,&shot,owner,expected,&h);expected=add(expected);
  assert(h.points==300&&h.event==(0x418|(owner<<7))&&h.sound==12&&h.combat==expected);
  assert(a[k].active==2&&a[k].state==3&&!a[k].phase&&a[k].owner==owner);
  assert(a[k].bonus_kind==9&&a[k].x==128&&a[k].y==128);
  assert(shot.active==2&&shot.state==2&&!shot.phase);
 }
 assert(!arcade_e_shot_targets(a,128,128,1));
}
for(x=0;x<65536;x++){
 ArcadeEActor a[2]={{0}};a[1].x=65532;a[1].y=250;a[1].active=65;a[1].state=255;
 assert(arcade_e_contact_target(a,x,250,1)==((u16)(x-65532+7)<=14?1:255));
 a[0]=a[1];assert(arcade_e_contact_target(a,x,250,1)==((u16)(x-65532+7)<=14?0:255));
}
for(y=0;y<256;y++)for(f=0;f<256;f++){
 ArcadeEActor a[2]={{0}};a[0].x=65535;a[0].y=253;a[0].active=f;a[0].state=255;
 assert(arcade_e_contact_target(a,65535,y,1)==(f&&!(f&2)&&(u8)(y-253+7)<=14?0:255));
 assert(arcade_e_contact_target(a,65535,y,2)==255);
}
for(k=0;k<256;k++)for(owner=0;owner<2;owner++)for(combat=0;combat<65536;combat+=31){
 ArcadeEActor a={0},before;ArcadeBonusAward h,old;unsigned sum=(combat+192)&65535;
 a.active=65;a.state=255;a.phase=255;a.bonus_kind=k;a.x=65535;a.y=255;before=a;
 memset(&h,0xa5,sizeof h);old=h;
 if(k>10){assert(!arcade_e_bonus_collect(&a,owner,combat,&h));assert(!memcmp(&a,&before,sizeof a)&&!memcmp(&h,&old,sizeof h));continue;}
 assert(arcade_e_bonus_collect(&a,owner,combat,&h));
 assert(a.active==2&&!a.phase&&a.owner==owner&&a.state==255&&a.bonus_kind==k&&a.x==65535&&a.y==255);
 assert(h.points==(k==10?20000:(k+1)*1000)&&h.event==(0x400|(k==10?0x2a:k+9)|(owner<<7)));
 assert(h.sound==5&&h.combat==(sum<0x3f00?sum:combat));
}
return 0;}
'''
(o/'test.c').write_text(c)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),str(r/'src/arcade_e_combat.c'),str(r/'src/arcade_difficulty.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'horizontal_cases':131072,'vertical_flag_state_cases':196608,'two_hit_sequences':131072,'contact_axis_cases':131072,'bonus_valid_invalid_cases':2*256*len(range(0,65536,31)),'scope':'ROM-guarded host contracts with sanitizers. E live integration and original CPU runtime comparison remain pending.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
