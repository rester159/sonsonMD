#!/usr/bin/env python3
"""Pinned arcade command gate and extracted production dispatch regression."""
from pathlib import Path
import hashlib,subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/demo-sound';o.mkdir(parents=True,exist_ok=True)
rom=b''.join((r/'arcade'/n).read_bytes() for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(rom).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
for address,hexbytes in [(0x415d,'d63ac42086083d975239'),(0x6347,'0d5226040d222712'),(0x6410,'86047e6341'),(0x6400,'0d221026ff8c86017e6339'),(0x6339,'c6808d121f89200e')]:
 expected=bytes.fromhex(hexbytes);assert rom[address-0x4000:address-0x4000+len(expected)]==expected
source=(r/'src/sound.c').read_text()
def function(name):
 start=source.index('void '+name+'(');p=source.index('{',start);depth=1;end=p+1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[start:end]
c='''#include <assert.h>
#include <stdint.h>
#include <string.h>
typedef uint8_t u8;
#define SND_CMD_COIN 1
static u8 g_attract;
static struct {u8 demoSound;} g_config;
static u8 ay[3][16],snd_1up_oneshot[1];
static struct {const u8 *base,*pos;unsigned wait;u8 active;} rp_1up;
static unsigned calls,last;
static void replay_start(u8 cmd){calls++;last=cmd;}
'''+function('snd_play_coin')+'\n'+function('snd_command')+'\n'+function('snd_play_1up')+'''
int main(void){
 for(unsigned demo=0;demo<2;demo++)for(unsigned enabled=0;enabled<2;enabled++) {
  g_attract=demo;g_config.demoSound=enabled;
  for(unsigned cmd=0;cmd<256;cmd++){
   calls=0;last=999;snd_command(cmd);
   assert(calls==(unsigned)(!demo||enabled));
   if(calls)assert(last==cmd);else assert(last==999);
  }
  calls=0;last=999;snd_play_coin();assert(calls==1 && last==1);
  memset(ay,0x55,sizeof(ay));rp_1up.base=rp_1up.pos=0;rp_1up.wait=123;rp_1up.active=0;
  snd_play_1up();
  if(demo&&!enabled){
   for(unsigned i=0;i<16;i++)assert(ay[2][i]==0x55);
   assert(!rp_1up.base&&!rp_1up.pos&&rp_1up.wait==123&&!rp_1up.active);
  }else{
   for(unsigned i=0;i<16;i++)assert(ay[2][i]==(i==7?255:0));
   assert(rp_1up.base==snd_1up_oneshot&&rp_1up.pos==snd_1up_oneshot&&!rp_1up.wait&&rp_1up.active);
  }
 }
 return 0;
}
'''
(o/'test.c').write_text(c)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(o/'test.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
assert 'replay_start(SND_CMD_STOP)' in function('snd_shutdown')
report={'command_cases':1024,'one_up_cases':4,'coin_bypass_cases':4,'scope':'Pinned source gate bytes and actual native public functions with replay stub; all commands, demo/live and DIP states. No chip output or listening claim. Forced source system sounds/coin path remain separately owned.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
