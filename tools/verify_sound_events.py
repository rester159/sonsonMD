#!/usr/bin/env python3
"""Check native event translation against the original main-CPU ROM table."""
from pathlib import Path
import re, subprocess, sys
root=Path(__file__).resolve().parents[1]
rom=(root/'arcade/ss.01e').read_bytes()
s=(root/'src/sound.c').read_text()
table=re.search(r'static const u8 event_to_latch\[25\] = \{(.*?)\};',s,re.S)
actual=bytes(int(x,16) for x in re.findall(r'0x[0-9A-Fa-f]+',table[1]))
assert actual==rom[0x23dd:0x23dd+25]
# The queue dispatch at $6364 uses the two-byte descriptors at $63AB.
assert rom[0x2370:0x2377]==bytes.fromhex('58 8e 63 ab ec 85 d7')
assert rom[0x23ab:0x23dd]==bytes(v for i in range(25) for v in (i,1))
def func(name):
 a=s.index('void '+name+'(');b=s.index('{',a);depth=1;e=b+1
 while depth: depth+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[a:e]
out=root/'out/feedback-fixes/events';out.mkdir(parents=True,exist_ok=True)
c='''#include <stdint.h>
#include <assert.h>
typedef uint8_t u8;
static u8 g_attract;
static struct {u8 demoSound;} g_config;
static unsigned calls,last;
static void replay_start(u8 c){calls++;last=c;}
'''+table[0]+func('snd_command')+'''
int main(void){
 const u8 expected[25]={'''+','.join(map(str,rom[0x23dd:0x23dd+25]))+'''};
 for(unsigned demo=0;demo<2;demo++)for(unsigned enabled=0;enabled<2;enabled++){
  g_attract=demo;g_config.demoSound=enabled;
  for(unsigned i=0;i<256;i++){
   calls=0;snd_command(i);assert(calls==(!demo||enabled)*(i<25));
   if(calls)assert(last==expected[i]);
  }
 }
 return 0;
}
'''
(out/'test.c').write_text(c)
if '--host' not in sys.argv:
 print('PASS: native command table matches both original 25-entry ROM tables; host harness generated (use --host to execute)')
 raise SystemExit(0)
subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(out/'test.c'),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True,timeout=30)
print('PASS: original 25-entry descriptor/command tables and 1,024 native dispatch/gate cases')
