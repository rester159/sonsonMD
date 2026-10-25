#!/usr/bin/env python3
"""Original Freeze gate ordering and actual native update prefix."""
from pathlib import Path
import hashlib,subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/freeze-dispatch';o.mkdir(parents=True,exist_ok=True)
rom=b''.join((r/'arcade'/n).read_bytes() for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(rom).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
for addr,hexbytes in [(0x9112,'cc0001d323dd23'),(0x412d,'b6300643973b39'),(0x9130,'963b85802608d602588e9141ad953b')]:
 expected=bytes.fromhex(hexbytes);assert rom[addr-0x4000:addr-0x4000+len(expected)]==expected
s=(r/'src/game.c').read_text();start=s.index('static u8 game_arcade_frozen(');end=s.index('    /* One shared input dispatch',start)
code='''#include <assert.h>
#include <stdint.h>
typedef uint8_t u8;typedef u8 bool;
#define MODE_ARCADE 0
#define MODE_HOME 1
#define FALSE 0
static u8 g_mode,g_survivalRun;
static struct {u8 dsw2;}g_config;
static uint32_t g_frameCounter;
static uint16_t clock_word;
static unsigned dispatched;
static void entity_arcade_clock_advance(void){clock_word++;}
'''+s[start:end]+'''dispatched++;return 0;}
int main(void){
 for(unsigned mode=0;mode<2;mode++)for(unsigned dip=0;dip<256;dip++)for(unsigned survival=0;survival<2;survival++){
  g_mode=mode;g_config.dsw2=dip;g_survivalRun=survival;
  for(unsigned seed=0;seed<4;seed++){
   uint16_t initial=(uint16_t[]){0,255,65535,32767}[seed];clock_word=initial;
   g_frameCounter=dispatched=0;game_update();
   assert(g_frameCounter==1);assert(clock_word==(uint16_t)(initial+!survival));
   assert(dispatched==(unsigned)(mode!=0||(dip&128)!=0));
  }
 }
 return 0;
}
'''
(o/'test.c').write_text(code)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(o/'test.c'),'-o',str(o/'test')],check=True);subprocess.run([str(o/'test')],check=True)
report={'cases':4096,'scope':'Pinned original clock/DIP/gate bytes; extracted native update prefix across all DIP bytes, modes, Survival flags and clock carries. Does not prove boot/menu IRQ ownership.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
