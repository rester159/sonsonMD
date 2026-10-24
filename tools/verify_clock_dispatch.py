#!/usr/bin/env python3
"""Check production update prefix owns clock before results dispatch."""
from pathlib import Path
import json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/clock-dispatch';o.mkdir(parents=True,exist_ok=True)
g=(r/'src/game.c').read_text();e=(r/'src/entity.c').read_text()
a=g.index('bool game_update(void)');b=g.index('    if (g_survivalRun)',a)
prefix=g[a:b]+'    return FALSE;\n}\n'
a=e.index('static u8 wave_clock,');b=e.index('static u8 formation_reservation',a);owner=e[a:b]
assert g.count('entity_arcade_clock_advance();')==1 and e.count('entity_arcade_clock_advance();')==0
loop=g[g.index('void game_run(void)'):]
assert loop.index('if (g_paused)')<loop.index('done = game_update();')
assert 'SYS_doVBlankProcess();\n            continue;' in loop
c=r'''
#include <assert.h>
#include "arcade_types.h"
typedef u8 bool;
#define FALSE 0
static u8 g_survivalRun,g_resultsActive,consumed;
static u32 g_frameCounter;
static unsigned calls;
static u16 observed;
OWNER
static u8 game_results_update(void){calls++;observed=(u16)((wave_clock_high<<8)|wave_clock);return consumed;}
PREFIX
int main(void){unsigned word,mode;for(word=0;word<65536;word++)for(mode=0;mode<8;mode++){
 wave_clock=word;wave_clock_high=word>>8;
 g_survivalRun=mode&1;g_resultsActive=mode&2;consumed=mode&4;
 calls=0;g_frameCounter=0;observed=0;
 game_update();
 unsigned expected=(word+(!g_survivalRun))&65535;
 assert((((unsigned)wave_clock_high<<8)|wave_clock)==expected);
 assert(g_frameCounter==1 && calls==(g_resultsActive?1u:0u));
 if(g_resultsActive)assert(observed==expected);
}
return 0;
}
'''.replace('OWNER',owner).replace('PREFIX',prefix)
# The archived pre-change entry has no clock advance. The same observer must
# reject it; this demonstrates the ordering regression independently of counts.
old=(r/'tools/fixtures/game_clock_before_predispatch.c').read_text()
old=old[old.index('bool game_update(void)'):]+ '    return FALSE;\n}\n'
(o/'before.c').write_text(c.replace(prefix,old))
subprocess.run(['cc','-std=c99','-O2','-I'+str(r/'inc'),str(o/'before.c'),'-o',str(o/'before')],check=True)
before=subprocess.run([str(o/'before')],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
assert before.returncode!=0, 'Old update entry unexpectedly satisfies pre-dispatch clock contract'

(o/'test.c').write_text(c)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'old_order_rejected':True,'cases':524288,'scope':'Actual game_update entry and byte owner, stub results observer; all counter words, normal/Survival and results pass-through/consume paths. Pause call ordering guarded statically; boot/menu cadence and target integration remain separate.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
