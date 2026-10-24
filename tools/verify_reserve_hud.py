#!/usr/bin/env python3
"""Check source reserve-icon cap/positions and actual extracted HUD writer."""
from pathlib import Path
import hashlib,json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/reserve-hud';o.mkdir(parents=True,exist_ok=True)
b=(r/'arcade/ss.01e').read_bytes()
assert b[0x1be+24:0x1be+26]==bytes.fromhex('51d8')
assert b[0x11f1:0x11fb]==bytes.fromhex('d6592714c1042302c604')
assert b[0x1216:0x1221]==bytes.fromhex('d6585d2734c1042302c604')
assert b[0x1200:0x1207]==bytes.fromhex('8e1097108e5258')
assert b[0x1226:0x122d]==bytes.fromhex('8e1082108e5250')
s=(r/'src/arcade_ui.c').read_text();s=s[s.index('void arcade_gameplay_lives('):]
c='''#include <stdint.h>
#include <string.h>
#include <assert.h>
typedef uint8_t u8;typedef uint16_t u16;
#define BG_B 1
#define PAL0 0
#define CPU 0
#define TILE_ATTR_FULL(p,pri,v,h,t) ((pri)*32768)
static char plane[4][32];static unsigned calls;
static void VDP_drawTextEx(int plane_id,const char*text,int attr,unsigned x,unsigned y,int method){
 assert(plane_id==BG_B && attr==32768 && method==CPU);assert(y==2||y==3);assert(x+strlen(text)<=32);
 memcpy(&plane[y][x],text,strlen(text));calls++;
}
'''+s+'''
int main(void){for(unsigned owner=0;owner<2;owner++){for(unsigned reserve=0;reserve<256;reserve++){
 memset(plane,'?',sizeof plane);calls=0;arcade_gameplay_lives(owner,reserve);assert(calls==2);
 unsigned col=owner?23:2,n=reserve>4?4:reserve;
 for(unsigned y=0;y<4;y++)for(unsigned x=0;x<32;x++){
  char expected='?';if(y>=2 && x>=col && x<col+8){unsigned i=x-col;
   expected=i<n*2?'p'+owner*4+(y-2)*2+(i%2):' ';}
  assert(plane[y][x]==expected);
 }
 /* Decreasing reserves must erase every icon cell, not leave stale tails. */
 arcade_gameplay_lives(owner,0);for(unsigned y=2;y<4;y++)for(unsigned x=col;x<col+8;x++)assert(plane[y][x]==' ');
}}
return 0;}
'''
(o/'test.c').write_text(c)
subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(o/'test.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'passed':True,'owner_reserve_cases':512,'source_sha256':hashlib.sha256(b).hexdigest(),'scope':'Source byte assertions for event0C, player addresses and four-icon cap. Extracted production writer checks every reserve byte for both owners, untouched neighboring cells and clearing after decrease. Host only; linked target pixels/map checked separately by verify_hud_format.py.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
