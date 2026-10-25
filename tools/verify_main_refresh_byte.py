#!/usr/bin/env python3
"""Check actual source23:24 owner and main-wave arguments against ROM arithmetic."""
from pathlib import Path
import re,hashlib,subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/shared-clock';o.mkdir(parents=True,exist_ok=True)
b=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(b).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
for addr,h in [(0xfff8,'910d'),(0x9112,'cc0001d323dd23'),(0xf029,'d623'),(0xf06f,'d623d824')]:
 assert b[addr-0x4000:addr-0x4000+len(bytes.fromhex(h))]==bytes.fromhex(h)
s=(r/'src/entity.c').read_text();a=s.index('static u8 wave_clock,');z=s.index('static u8 formation_reservation',a)
hi,lo=re.search(r'arcade_main_pool_schedule\(&arcade_main,arcade_main_ready,(.*?),(.*?),',s).groups()
assert hi=='wave_clock_high' and lo=='wave_clock'
assert 'vtimer' not in s
assert 'wave_clock=0;' not in s and 'wave_clock++;' not in s
assert s.count('entity_arcade_clock_advance();')==0
c='''#include <assert.h>
#include "arcade_types.h"
OWNER
int main(void){unsigned word,step;for(word=0;word<65536;word++){
 wave_clock=word;wave_clock_high=word>>8;
 assert((((unsigned)HI<<8)|LO)==word);
 /* Repeated reads/rendering do not advance either byte. */
 for(step=0;step<4;step++)assert(entity_arcade_clock()==(word&255));
 entity_arcade_clock_advance();
 assert((((unsigned)HI<<8)|LO)==((word+1)&65535));
}
wave_clock=wave_clock_high=0;
for(step=0;step<200000;step++){
 assert((((unsigned)HI<<8)|LO)==(step&65535));entity_arcade_clock_advance();
}
return 0;}
'''.replace('OWNER',s[a:z]).replace('HI',hi).replace('LO',lo)
(o/'test.c').write_text(c)
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'all_counter_states':65536,'consecutive_steps':200000,'scope':'Actual native byte owner and main-wave arguments; guarded original16-bit increment and source loads. Carry/wrap/read stability pass. Boot/menu IRQ phase and sustained55.4Hz service remain incomplete; no claim of full timing parity.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
