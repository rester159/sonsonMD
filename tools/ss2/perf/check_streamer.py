#!/usr/bin/env python3
"""Compile the real C streamer with a recording VDP; check all visible cells.

Usage: check_streamer.py [source.c]. No SGDK/emulator dependency.
"""
import pathlib, subprocess, tempfile, sys
source=pathlib.Path(sys.argv[1] if len(sys.argv)>1 else 'src/ss2_main.c').read_text()
body=source[source.index('static s16 camx, camy;'):source.index('/* plane B shares plane A')]
pre=r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
typedef uint8_t u8; typedef uint16_t u16; typedef int16_t s16;
#define NCOLS 33
#define NROWS 23
#define BG_TILE 17
#define TILE_ATTR_PRIORITY_MASK 0x8000
#define BG_A 0
#define CPU 0
static u8 ram[65536];
#define R(a) ram[(u16)(a)]
static u16 meta[4096], vram[32][64];
static struct {u16 *meta;} graphics={meta}, *gfx=&graphics;
static unsigned writes;
static void VDP_setTileMapDataRow(int p,const u16*d,u16 y,u16 x,u16 n,int mode) {
 for(unsigned i=0;i<n;i++){vram[y&31][(x+i)&63]=d[i];writes++;}
}
static void VDP_setTileMapDataColumn(int p,const u16*d,u16 x,u16 y,u16 n,u16 stride,int mode) {
 for(unsigned i=0;i<n;i++){vram[(y+i)&31][x&63]=d[i*stride];writes++;}
}
'''
post=r'''
static unsigned checks;
static void check(void) {
 for(unsigned y=0;y<NROWS;y++)for(unsigned x=0;x<NCOLS;x++){
  unsigned tx=((camx>>3)+x)&255,ty=((camy>>3)+y)&255;
  /* Independent screen/metatile/quadrant decomposition. */
  unsigned screen=(ty/32)*8+tx/32;
  unsigned m=ram[0x3000+screen*64+((ty%32)/4)*8+(tx%32)/4];
  u16 expected=(meta[m*16+(ty%4)*4+tx%4]+BG_TILE)|0x8000;
  if(vram[ty%32][tx%64]!=expected){fprintf(stderr,"mismatch camera %d,%d tile %u,%u\n",camx,camy,tx,ty);exit(1);}
  checks++;
 }
}
int main(void){
 for(unsigned i=0;i<4096;i++){ram[0x3000+i]=(i*73+i/7)&255;meta[i]=(i*137+i/11)&0x1fff;}
 camx=0;camy=0;redraw_view();check();
 writes=0;camy=2040;stream_view();check();unsigned wrap=writes;
 /* Every horizontal/vertical phase, both directions, diagonal and world wrap. */
 for(unsigned phase=0;phase<256;phase++){
  camx=phase*8;camy=phase*8;redraw_view();
  for(unsigned i=0;i<512;i++){
   camx=(camx+(i<256?8:2040))&2047;
   camy=(camy+(i<256?2040:8))&2047;
   stream_view();check();
  }
 }
 for(unsigned i=0;i<500;i++){
  camx=(i*631)&2047;camy=(i*997)&2047;stream_view();check();
 }
 printf("%u visible-cell comparisons passed; upward-wrap writes: %u\n",checks,wrap);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=pathlib.Path(d);(p/'check.c').write_text(pre+body+post)
 subprocess.run(['cc','-O2','-fsanitize=undefined,address',str(p/'check.c'),'-o',str(p/'check')],check=True)
 subprocess.run([str(p/'check')],check=True)
