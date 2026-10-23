#!/usr/bin/env python3
from pathlib import Path
import subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/gameplay-input-latch';o.mkdir(parents=True,exist_ok=True)
s=(r/'src/game.c').read_text();a=s.index('static u16 gameplay_held');b=s.index('/* Returns true when results own',a)
c=r'''
#include <assert.h>
#include "arcade_types.h"
#define JOY_1 0
#define JOY_2 1
static u16 hardware[2];
u16 JOY_readJoypad(u8 k){return hardware[k];}
HELPERS
int main(void){unsigned initial,press,released,discard;unsigned cases=0;
for(initial=0;initial<16;initial++)for(press=0;press<16;press++)
for(released=0;released<16;released++)for(discard=0;discard<2;discard++){
 for(unsigned k=0;k<2;k++){gameplay_held[k]=initial<<(k*4);gameplay_pressed[k]=0;hardware[k]=press<<(k*4);}
 gameplay_input_sample(0);
 for(unsigned k=0;k<2;k++)hardware[k]=released<<(k*4);
 gameplay_input_sample(discard);
 gameplay_input_consume();
 for(unsigned k=0;k<2;k++){
  unsigned expected=discard?released:(released|(press&~initial));
  assert(gameplay_pad[k]==(expected<<(k*4)) && !gameplay_pressed[k]);
 }
 gameplay_input_consume();
 for(unsigned k=0;k<2;k++)assert(gameplay_pad[k]==(released<<(k*4)));
 cases++;
}
gameplay_input_ready=0;assert(cases==8192);return 0;
}
'''.replace('HELPERS',s[a:b])
(o/'test.c').write_text(c)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'cases':8192,'scope':'Production input helpers with pad shim: transient presses persist until consumed, second consumption uses held state, pause discards queued edges, independent controllers. Multiple taps between simulation polls coalesce; not original board input-phase proof.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
