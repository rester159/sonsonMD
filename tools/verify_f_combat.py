#!/usr/bin/env python3
"""F combat source contracts; no emulator frontend or original execution claim."""
from pathlib import Path
import hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/f-combat';o.mkdir(parents=True,exist_ok=True)
b=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(b).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
def guard(addr,data):assert b[addr-0x4000:addr-0x4000+len(bytes.fromhex(data))]==bytes.fromhex(data)
guard(0x643d,'86 0d 7e 63 41 86 0e 7e 63 41')
guard(0xe89c,'09 0a 0b 0c 0d 0e 0f 10 11 12 2a')
guard(0xeb97,'8e 0b a8 e6 30 5a 26 25 c6 04 d7 c8 e6 38 cb 05 d7 c9 ec 35 c3 00 08 dd ca e6 10 5a 26 08 e6 11 2b 04 8d 0a 25 07 30 88 20 0a c8 26 ec 39')
guard(0xebe1,'c6 02 e7 10 e7 30 cc 01 00 ed 11 cc 02 00 ed 31 34 30 cc 04 19 da d5 bd 96 04 bd 64 38 d6 d5 1d 84 01 a7 14 cc 00 33 bd e4 07 0a b6 26 16 cc 01 04 ed 11 cc 04 13 da d5 bd 96 04 bd 64 42 cc 00 40 bd e4 07 1a 01 35 b0')
guard(0xed79,'ce 0b a8 c6 04 7e ec e9')
guard(0x6438,'86 0c 7e 63 41')
guard(0x4cd9,'a6 82 ab a2 19 a7 84 a6 82 a9 a2 19 a7 84 a6 82 a9 a2 19 a7 84 a6 82 a9 a2 19 a7 84')
for event,points in [(0x19,50),(0x13,500),*[(k+9,(k+1)*1000)for k in range(10)],(0x2a,20000)]:
 addr=0x4d84+event*2;end=int.from_bytes(b[addr-0x4000:addr-0x4000+2],'big');assert int(b[end-4-0x4000:end-0x4000].hex())==points
c=r'''
#include <assert.h>
#include <string.h>
#include "arcade_f_combat.h"
static unsigned add(unsigned x,unsigned n){unsigned y=(x+n)&65535;return y<0x3f00?y:x;}
int main(void){unsigned owner,count,combat,x,y,flag,kind;
for(owner=0;owner<2;owner++)for(count=0;count<256;count++)for(combat=0;combat<65536;combat+=31){
 ArcadeFActor a={0};ArcadePlayerShot s={0};ArcadeFHit h;u8 remaining=count;
 memset(&h,255,sizeof h);a.origin_y=17;a.bonus_kind=9;a.x=65535;a.y=255;
 arcade_f_hit(&a,&s,owner,&remaining,combat,&h);
 assert(remaining==(u8)(count-1));assert(a.active==2&&a.state==1&&a.phase==(count==1?4:0)&&a.owner==owner);
 assert(a.origin_y==17&&a.bonus_kind==9&&a.x==65535&&a.y==255);
 assert(s.active==2&&s.state==2&&!s.phase);
 assert(h.points==50&&h.event==(0x419|(owner<<7))&&h.sound==12);
 assert(h.bonus_points==(count==1?500:0)&&h.bonus_event==(count==1?(0x413|(owner<<7)):0)&&h.bonus_sound==(count==1?14:0));
 assert(h.combat==(count==1?add(add(combat,0x33),0x40):add(combat,0x33)));
}
/* Independent center calculations, each axis swept across all wrapped values. */
for(x=0;x<65536;x++)for(y=0;y<256;y+=255){
 ArcadeFActor a[4]={0};unsigned i;u8 expected=255;
 a[3].active=1;a[3].x=65532;a[3].y=y;
 for(i=0;i<4;i++)if(a[i].active==1 && (u8)((u8)(a[i].y+8)-(u8)(y+5)+8)<=16 && (u16)((u16)(a[i].x+8)-(u16)(x+8)+8)<=16){expected=i;break;}
 assert(arcade_f_shot_target(a,x,y,1)==expected);
 a[0]=a[3];assert(arcade_f_shot_target(a,x,y,1)==(expected==255?255:0));
 a[0].state=128;assert(arcade_f_shot_target(a,x,y,1)==expected);
}
for(y=0;y<256;y++)for(flag=0;flag<256;flag++){
 ArcadeFActor a[4]={0};a[0].active=flag;a[0].state=255;a[0].x=65535;a[0].y=253;
 assert(arcade_f_shot_target(a,65535,y,1)==255);
 a[0].state=0;
 assert(arcade_f_shot_target(a,65535,y,1)==(flag==1&&(u8)(253-y+11)<=16?0:255));
 assert(arcade_f_contact_target(a,65535,y,1)==(flag&&!(flag&2)&&(u8)(y-253+7)<=14?0:255));
 assert(arcade_f_contact_target(a,65535,y,2)==255);
 assert(arcade_f_shot_target(a,65535,y,2)==255);
}
for(kind=0;kind<11;kind++)for(owner=0;owner<2;owner++){
 ArcadeFActor a={0};ArcadeBonusAward award;a.bonus_kind=kind;a.origin_y=99;a.phase=255;
 assert(arcade_f_bonus_collect(&a,owner,0x3eff,&award));
 assert(a.active==2&&!a.phase&&a.owner==owner&&a.origin_y==99);
 assert(award.points==(kind==10?20000:(kind+1)*1000));
 assert(award.event==(0x400|(kind==10?0x2a:kind+9)|(owner<<7))&&award.sound==5&&award.combat==0x3eff);
}
return 0;}
'''
(o/'test.c').write_text(c)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),str(r/'src/arcade_f_combat.c'),str(r/'src/arcade_difficulty.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'hit_cases':2*256*len(range(0,65536,31)),'horizontal_cases':65536*2,'flag_vertical_cases':256*256,'bonus_cases':22,'scope':'Offline ROM-guarded host contracts with sanitizers; not live F integration or original CPU execution.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
