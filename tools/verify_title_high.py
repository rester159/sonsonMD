#!/usr/bin/env python3
"""Check the Home title score writer against the saved arcade field bounds."""
from pathlib import Path
import json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/title-high-fix';o.mkdir(exist_ok=True)
scene=json.loads((r/'tools/title_capture.json').read_text())
assert scene['colorram'][2*32+13:2*32+21]==[44]*8
s=(r/'src/title.c').read_text();a=s.index('static void title_high(void)\n{');b=s.index('static void home_choices',a)
c='''#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
typedef uint8_t u8;typedef uint16_t u16;typedef int32_t s32;typedef uint32_t u32;
#define BG_A 0
#define PAL0 0
#define TILE_ATTR_FULL(p,pr,v,h,t) (t)
static u8 g_mode;static u32 high;static u16 map[32*32];
static u32 save_mode_high(u8 mode){(void)mode;return high;}
static void intToStr(s32 n,char*s,u16 width){sprintf(s,"%0*d",width,n);}
static u16 glyph(char c,u8 red){(void)red;return (u16)c;}
static void VDP_setTileMapXY(u16 plane,u16 tile,u16 x,u16 y){assert(!plane&&x<32&&y<32);map[y*32+x]=tile;}
'''+s[a:b]+'''
int main(void){
 const u32 values[]={0,1,9999,10000,999999,1000000,99999999,100000000,4294967295u};
 for(unsigned t=0;t<sizeof(values)/sizeof(*values);t++){
  char expected[12];high=values[t];sprintf(expected,"%8u",high%100000000u);
  for(unsigned i=0;i<1024;i++)map[i]=0x1234;
  title_high();
  for(unsigned i=0;i<1024;i++)assert(map[i]==(i>=45&&i<53?(u16)expected[i-45]:0x1234));
 }
}
'''
(o/'check.c').write_text(c)
subprocess.run(['cc','-O2',str(o/'check.c'),'-o',str(o/'check')],check=True)
subprocess.run([str(o/'check')],check=True)
print('Nine score boundaries pass; only source row2 columns13..20 change.')
