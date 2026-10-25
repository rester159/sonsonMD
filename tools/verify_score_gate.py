#!/usr/bin/env python3
"""Production score-write helper: live gate, ownership and reward evaluation."""
from pathlib import Path
import re,subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/score-gate';o.mkdir(parents=True,exist_ok=True)
header=(r/'inc/game.h').read_text();helper=re.search(r'static inline void game_score_add\(.*?\n\}',header,re.S).group()
c='''#include <stdint.h>
#include <assert.h>
typedef uint32_t u32;typedef uint8_t u8;
typedef struct {u32 score;} Player;
static volatile u8 g_attract;
static unsigned calls;
static u32 reward(u32 n){++calls;return n;}
'''+helper+'''
int main(void){
 const u32 scores[]={0,1,19999,99999999,0xffffffff};
 const u32 points[]={0,30,150,1000,20000,0xffffffff};
 for(unsigned demo=0;demo<256;demo++)for(unsigned owner=0;owner<2;owner++)
 for(unsigned s=0;s<5;s++)for(unsigned p=0;p<6;p++){
  Player players[2]={{scores[s]},{scores[s]}};g_attract=demo;calls=0;
  game_score_add(&players[owner],reward(points[p]));
  assert(calls==1);assert(players[owner^1].score==scores[s]);
  assert(players[owner].score==(demo?scores[s]:(u32)(((uint64_t)(scores[s]%100000000u)+(points[p]%100000000u))%100000000u)));
 }
 return 0;
}
'''
(o/'test.c').write_text(c)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(o/'test.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
for n in ('src/game.c','src/entity.c'):
 assert not re.search(r'(?:\.score|->score)\s*\+=',(r/n).read_text()),n
report={'passed':True,'cases':15360,'scope':'Extracted production inline helper under ASan/UBSan; both score owners, all demo-byte values, eight-digit live arithmetic and side-effecting reward evaluation. Source scan confirms game/entity additive awards route through helper. Independent packed-BCD rollover checks are in verify_score_rollover.py.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
