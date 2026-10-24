#!/usr/bin/env python3
"""ROM-backed independent coin-slot ratios and fraction ownership; native UI cap is separate."""
from pathlib import Path
import hashlib,subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/coin-slots';o.mkdir(parents=True,exist_ok=True)
rom=b''.join((r/'arcade'/n).read_bytes() for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(rom).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
for address,hexbytes in [(0x4134,'cc0101fd0033fd0038'),(0x4149,'488e423aae86d63ac5102605bf00332003bf0038'),(0x921c,'d6c8c51027058e00308d1bd6c8c52027058e00358d10'),(0x501f,'bd4fa2c610e784e701e702e703e704e705'),(0x5046,'c606a6c4260833415a26f7a78439a6c0a7805a26f939'),(0x9242,'6c846c01a601a10325f5e6044fd3252502dd256f0139')]:
 expected=bytes.fromhex(hexbytes);assert rom[address-0x4000:address-0x4000+len(expected)]==expected,hex(address)
table=rom[0x423a-0x4000:0x425a-0x4000]
source=(r/'src/mode_controls.c').read_text();source='\n'.join(x for x in source.splitlines() if not x.startswith('#include'))
c='''#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef uint8_t u8;typedef uint16_t u16;typedef int bool;
#define TRUE 1
#define FALSE 0
#define MODE_ARCADE 0
#define JOY_1 0
#define JOY_2 1
#define BUTTON_C 32
#define BUTTON_START 128
static u8 g_mode;
static struct {u8 dsw1;} g_config;
static u16 pads[2];static u16 JOY_readJoypad(unsigned p){return pads[p];}
'''+source+'\nstatic const u8 table[32]={'+','.join(str(x) for x in table)+'''};
int main(void){unsigned raw,seq,step,cases=0;uint32_t seed=1;
for(raw=0;raw<256;raw++)for(seq=0;seq<64;seq++){
 u8 fractions[2]={0,0};u16 oldpads[2]={0,0};unsigned expected=0;
 g_config.dsw1=raw;g_mode=0;g_arcadeCredits=0;memset(coin_fraction,0,sizeof coin_fraction);pads[0]=pads[1]=0;mode_input_rearm();
 for(step=0;step<512;step++){
  unsigned k;u8 starts=0,coinmask=0;
  for(k=0;k<2;k++){
   seed=seed*1664525u+1013904223u;pads[k]=(seed>>24)&(BUTTON_C|BUTTON_START);
   unsigned edge=pads[k]&~oldpads[k];oldpads[k]=pads[k];
   if(edge&BUTTON_START)starts|=1<<k;
   if(edge&BUTTON_C){
    coinmask|=1<<k;
    unsigned selected=((~raw)&16)?1:0,need=1,award=1;
    if(k==selected){unsigned offset=((~raw)&15)*2;need=table[offset];award=table[offset+1];}
    fractions[k]++;
    if(fractions[k]>=need){fractions[k]=0;if(expected+award<=65535)expected+=award;}
   }
  }
  assert(mode_input_poll()==starts);assert(mode_input_coins()==coinmask);assert(mode_input_starts_held()==(((pads[0]&BUTTON_START)?1:0)|((pads[1]&BUTTON_START)?2:0)));assert(g_arcadeCredits==expected);
  assert(!memcmp(fractions,coin_fraction,2));cases++;
  if(step%16==15){g_arcadeCredits=0;expected=0;}
 }
}
/* Home C cannot alter arcade credit or fractional ownership. */
g_mode=1;g_arcadeCredits=7;coin_fraction[0]=2;coin_fraction[1]=3;pads[0]=pads[1]=0;mode_input_rearm();pads[0]=pads[1]=BUTTON_C;mode_input_poll();
assert(g_arcadeCredits==7 && coin_fraction[0]==2 && coin_fraction[1]==3);
assert(cases==8388608);
for(raw=0;raw<32;raw++)for(unsigned initial=0;initial<65536;initial++){
 unsigned slot=(raw&16)?0:1,offset=((~raw)&15)*2,need=table[offset],award=table[offset+1];
 unsigned expected=initial+award<=65535?initial+award:initial;
 g_config.dsw1=raw;g_mode=0;g_arcadeCredits=initial;coin_fraction[0]=coin_fraction[1]=0;coin_fraction[slot]=need-1;
 pads[0]=pads[1]=0;mode_input_rearm();pads[slot]=BUTTON_C;mode_input_poll();
 assert(g_arcadeCredits==expected && !coin_fraction[slot]);
 bool accepted=arcade_take_credit();
 if(!(raw&15)){assert(accepted && g_arcadeCredits==expected);}
 else if(expected){assert(accepted && g_arcadeCredits==expected-1);}
 else assert(!accepted && g_arcadeCredits==0);
}
for(unsigned value=0;value<65536;value++){
 char got[7],expected[7],digits[7];unsigned length;
 memset(expected,' ',6);expected[6]=0;snprintf(digits,sizeof digits,"%u",value);length=strlen(digits);memcpy(expected,digits,length);
 arcade_credit_text(value,got);assert(!memcmp(got,expected,7));
}
return 0;}
'''
(o/'test.c').write_text(c)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(o/'test.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'poll_cases':8388608,'word_add_purchase_cases':2097152,'format_cases':65536,'source_sha256':hashlib.sha256(rom).hexdigest(),'scope':'Actual native control module with pad shim versus pinned original ROM ratio table/setup/independent fraction ownership. All raw DIP bytes, mixed slot edges, held inputs, Start edges, and Home non-coin behavior. Word credit addition discards overflow as in source. Does not validate sound, original switch debounce, complete cabinet lifecycle or target audio.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
