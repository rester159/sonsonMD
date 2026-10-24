#!/usr/bin/env python3
"""Source-pinned combined-score packing across the eight-digit wrap boundary."""
from pathlib import Path
import re,subprocess,json,hashlib
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/combined-score';o.mkdir(exist_ok=True)
rom=(r/'arcade/ss.01e').read_bytes();code=rom[0xd66:0xd82]
assert code.hex()=='a682abc219a7a2a682a9c219a7a2a682a9c219a7a2a682a9c219a7a2'
s=(r/'src/arcade_postgame.c').read_text();assert 'pack_score(p1+p2,scores[2]);' in s
fn=re.search(r'static void pack_score\(.*?\n\}',s,re.S).group()
c='''#include <stdint.h>
#include <assert.h>
typedef uint32_t u32;typedef uint8_t u8;typedef int16_t s16;
'''+fn+'''
static void original_sum(u32 a,u32 b,u8 out[4]){
 unsigned carry=0;int k;
 for(k=3;k>=0;k--){unsigned pair=a%100+b%100+carry;out[k]=((pair%100)/10)*16+pair%10;carry=pair>=100;a/=100;b/=100;}
}
int main(void){u32 seed=7;unsigned n;
 for(n=0;n<200000;n++){
  seed=seed*1664525u+1013904223u;u32 a=seed%100000000u;
  seed=seed*1664525u+1013904223u;u32 b=seed%100000000u;
  if(n<100000){a=99999999u-n;b=n+1;}
  u8 actual[4],expected[4];pack_score(a+b,actual);original_sum(a,b,expected);
  for(unsigned k=0;k<4;k++)assert(actual[k]==expected[k]);
 }
 return 0;
}
''';(o/'test.c').write_text(c)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(o/'test.c'),'-o',str(o/'test')],check=True);subprocess.run([str(o/'test')],check=True)
report={'cases':200000,'source_range':'4D66..4D81','source_bytes':code.hex(),'rom_sha256':hashlib.sha256(rom).hexdigest(),'scope':'Actual postgame pack_score and verified call p1+p2 vs independent four-byte decimal carry sum. Valid individual scores 0..99999999, so binary intermediate cannot overflow u32. Host test; live total display/update timing remains separate.'}
(o/'report.json').write_text(json.dumps(report,indent=2));print(report)
