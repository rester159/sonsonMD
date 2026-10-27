#!/usr/bin/env python3
"""Exercise production shop_exit with a recording SAT, including its busy waits.
An optional source path allows the old implementation to demonstrate failure.
"""
from pathlib import Path
import subprocess, tempfile, sys
root=Path(__file__).resolve().parents[2]
s=(Path(sys.argv[1]) if len(sys.argv)>1 else root/'src/ss2_npc.c').read_text()
def function(name):
 start=s.index('static void '+name+'(');brace=s.index('{',start);depth=1;i=brace+1
 while depth:
  depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[start:i]
pre=r'''
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ss2_ram.h"
u8 ss2_ram[0x2000], ss2_shop_screen, npc_bank;
u16 ss2_sat[64*4]; u8 ss2_sat_hi, ss2_redraw_request;
static unsigned waits, failures;
static void ss2_fade_out_alt(void) {}
static void ss2_fade_in(void) {}
static void ss2_sfx(u8 x) {}
static void ss2_map6(u8 x) {}
static void ss2_busy(u8 frames) {
 if(ss2_shop_screen || ss2_sat_hi) failures++;
 for(unsigned i=0;i<256;i++)if(ss2_sat[i]) failures++;
 if(frames!=(waits?5:3))failures++;
 waits++;
}
'''
post=r'''
int main(void) {
 for(unsigned n=1;n<=64;n++){
  memset(ss2_ram,0,sizeof ss2_ram);
  for(unsigned i=0;i<256;i++)ss2_sat[i]=i<n*4?(0x100+i):0;
  ss2_sat_hi=n;ss2_shop_screen=1;waits=0;ss2_redraw_request=0;
  for(unsigned i=0;i<6;i++)ZP(0x57+i)=i*37;
  shop_exit();
  if(waits!=2 || !ss2_redraw_request)failures++;
  for(unsigned i=0;i<6;i++)if(ZP(0x45+i)!=ZP(0x57+i))failures++;
 }
 if(failures){fprintf(stderr,"FAIL: %u stale sprite/transition observations\n",failures);return 1;}
 puts("PASS: 64 SAT populations cleared before stage restoration; camera and redraw preserved");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(pre+function('vdc')+function('shop_exit')+post)
 subprocess.run(['cc','-O1','-fsanitize=address,undefined','-I'+str(root/'tools/ss2/hosttest/shim'),'-I'+str(root/'inc'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
