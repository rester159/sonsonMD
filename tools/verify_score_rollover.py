#!/usr/bin/env python3
"""Pin the four-byte source DAA chain and compare the production decimal helper."""
from pathlib import Path
import hashlib,re,subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/score-rollover';o.mkdir(exist_ok=True)
rom=(r/'arcade/ss.01e').read_bytes();code=rom[0xcd9:0xcf5]
assert code.hex()=='a682aba219a784a682a9a219a784a682a9a219a784a682a9a219a784',code.hex()
helper=re.search(r'static inline void game_score_add\(.*?\n\}',(r/'inc/game.h').read_text(),re.S).group()
c='''#include <stdint.h>
#include <assert.h>
typedef uint32_t u32;typedef uint8_t u8;
typedef struct{u32 score;}Player;
static volatile u8 g_attract;
'''+helper+'''
/* Independent valid packed-BCD oracle: carry moves from the low decimal pair
 * through four source bytes. The carry after byte 3 is not stored. */
static u32 bcd_add(u32 a,u32 b){
 u32 result=0,place=1;unsigned carry=0,k;
 for(k=0;k<4;k++){
  unsigned av=a%100,bv=b%100;
  unsigned packed_a=((av/10)<<4)|(av%10),packed_b=((bv/10)<<4)|(bv%10);
  unsigned low=(packed_a&15)+(packed_b&15)+carry;
  unsigned high=(packed_a>>4)+(packed_b>>4)+(low>=10);
  result+=((high%10)*10+low%10)*place;
  carry=high>=10;a/=100;b/=100;place*=100;
 }
 return result;
}
int main(void){
 u32 seed=0x12345678;unsigned n;g_attract=0;
 for(n=0;n<200000;n++){
  seed=seed*1664525u+1013904223u;u32 a=seed;
  seed=seed*1664525u+1013904223u;u32 b=seed;
  if(n<100000){a=99900000u+n;b=n%20001;}
  Player p={a};game_score_add(&p,b);assert(p.score==bcd_add(a,b));
 }
 Player p={99999990};game_score_add(&p,30);assert(p.score==20);
 p.score=99999999;game_score_add(&p,1);assert(p.score==0);
 return 0;
}
''';(o/'test.c').write_text(c)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(o/'test.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'cases':200002,'source_range':'4CD9..4CF4','source_bytes':code.hex(),'rom_sha256':hashlib.sha256(rom).hexdigest(),'scope':'Production helper vs independent four-byte decimal carry oracle, pinned ADDA/ADCA+DAA source chain. Includes wrap boundary and u32 external-value normalization. Not original-CPU execution, bonus-life timing or total-ranking rollover proof.'}
(o/'report.json').write_text(json.dumps(report,indent=2));print(report)
