#!/usr/bin/env python3
"""Offline production A807 renderer and decoded-pixel checks."""
from pathlib import Path
import ctypes as C
import json,subprocess
import gen_assets as G
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/finish-background';o.mkdir(parents=True,exist_ok=True)
(o/'genesis.h').write_text("""#pragma once
#include <stdint.h>
typedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;
#define DMA 0
#define DMA_VRAM 0
void VDP_loadTileData(const u32*,u16,u16,u16);
u16 DMA_getQueueSize(void);u16 DMA_getMaxQueueSize(void);u16 DMA_canQueue(u16,u16);
u16 VDP_getBGAAddress(void);u16 DMA_queueDma(u16,void*,u16,u16,u16);
""")
(o/'shim.c').write_text("""#include <assert.h>
#include <genesis.h>
u16 count,capacity=4,allowed=1,addresses[4],values[4];
void VDP_loadTileData(const u32*p,u16 base,u16 n,u16 mode){(void)p;(void)mode;assert(base==1976 && n==8);}
u16 DMA_getQueueSize(void){return count;}u16 DMA_getMaxQueueSize(void){return capacity;}
u16 DMA_canQueue(u16 t,u16 n){(void)t;assert(n==4);return allowed;}
u16 VDP_getBGAAddress(void){return 0xe000;}
u16 DMA_queueDma(u16 t,void*p,u16 a,u16 n,u16 step){(void)t;assert(n==1 && step==2 && count<4);addresses[count]=a;values[count]=*(u16*)p;count++;return 1;}
u8 vram_words_room(u16 n){assert(n==4);return allowed && count+n<=capacity;}
u8 vram_words_queue(const u16*p,u16 a,u16 n,u16 stride){assert(n==1 && stride==2 && count<4);addresses[count]=a;values[count]=*p;count++;return 1;}
""")
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-DSGDK_GCC','-dynamiclib','-I'+str(o),'-I'+str(r/'inc'),str(o/'shim.c'),str(r/'src/arcade_finish_background.c'),str(r/'src/arcade_finish_art.c'),'-o',str(o/'test.dylib')],check=True)
lib=C.CDLL(str(o/'test.dylib'));lib.arcade_finish_background_write.argtypes=[C.c_uint16,C.c_uint8,C.c_uint8]
def scalar(n):return C.c_uint16.in_dll(lib,n)
addresses=(C.c_uint16*4).in_dll(lib,'addresses');values=(C.c_uint16*4).in_dll(lib,'values')
checks=0
for phase in range(64):
 for camera in (0,159,160,167,168,255,256,3840,22016,22271):
  for pose in range(2):
   scalar('count').value=0;lib.arcade_finish_background_reset()
   lib.arcade_finish_background_render();assert scalar('count').value==0
   lib.arcade_finish_background_write(camera,phase,pose)
   scalar('capacity').value=3;lib.arcade_finish_background_render();assert scalar('count').value==0
   scalar('capacity').value=4;scalar('allowed').value=0;lib.arcade_finish_background_render();assert scalar('count').value==0
   scalar('allowed').value=1;lib.arcade_finish_background_render();assert scalar('count').value==4
   world=next(x for x in range(camera//8,camera//8+32)if x%32==20)
   expected=[0xe000+2*(y*64+(x+phase)%64)for x,y in ((world,7),(world,6),(world+1,7),(world+1,6))]
   assert list(addresses)==expected
   assert list(values)==list(range(1976+pose*4,1980+pose*4))
   scalar('count').value=0;lib.arcade_finish_background_render();assert scalar('count').value==0
   checks+=4
rom=b''.join((r/'arcade'/n).read_bytes()for n in ('ss.01e','ss.02e','ss.03e'));g=G.Roms(str(r/'arcade'))
pixels=list((C.c_uint32*64).in_dll(lib,'arcade_finish_pixels'))
for pose in range(2):
 pattern=rom[0x681f+pose*8:0x6827+pose*8]
 for k in range(4):
  a=pattern[k+4]
  assert pixels[(pose*4+k)*8:(pose*4+k+1)*8]==list(G.bake_char_tile(g,pattern[k]+((a&3)<<8),a>>2))
report={'map_words_checked':checks,'pixel_cells':8,'scope':'Production A807 background renderer offline: all64 plane phases, ten camera positions, both poses, admission retry, reset/retirement, exact source pixels. Live VDP/arcade alignment remains unverified.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
