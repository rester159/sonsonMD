#!/usr/bin/env python3
"""Exercise production release renderer with a bounded fake map-word queue, offline."""
from pathlib import Path
import ctypes as C
import json,subprocess,re
import gen_assets as G
r=Path(__file__).resolve().parents[1]
o=r/'out/parity-audit/release-background';o.mkdir(parents=True,exist_ok=True)
(o/'genesis.h').write_text('''#pragma once
#include <stdint.h>
#include <string.h>
typedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;
typedef int16_t s16;typedef int32_t s32;
#define DMA 0
#define DMA_VRAM 0
void VDP_loadTileData(const u32*,u16,u16,u16);
u16 DMA_getQueueSize(void);u16 DMA_getMaxQueueSize(void);
u16 DMA_canQueue(u16,u16);u16 VDP_getBGAAddress(void);
u16 DMA_queueDma(u16,void*,u16,u16,u16);
''')
(o/'shim.c').write_text('''#include <genesis.h>
u16 capacity=128,allowed=1,count,loaded_base,loaded_count;
u16 addresses[128],values[128];const u16 *sources[128];
void VDP_loadTileData(const u32*p,u16 b,u16 n,u16 m){(void)p;(void)m;loaded_base=b;loaded_count=n;}
u16 DMA_getQueueSize(void){return count;}
u16 DMA_getMaxQueueSize(void){return capacity;}
u16 DMA_canQueue(u16 t,u16 n){(void)t;(void)n;return allowed;}
u16 VDP_getBGAAddress(void){return 0xe000;}
u16 DMA_queueDma(u16 t,void*p,u16 a,u16 n,u16 step){
 (void)t;(void)n;(void)step;
 if(count>=capacity)return 0;
 addresses[count]=a;sources[count]=p;values[count]=*(u16*)p;count++;return 1;
}
/* Production map words use the VBlank word list (vram_words.h). */
u8 vram_words_room(u16 n){return allowed && count+n<=capacity;}
u8 vram_words_queue(const u16*p,u16 a,u16 n,u16 stride){
 (void)n;(void)stride;
 if(count>=capacity)return 0;
 addresses[count]=a;sources[count]=p;values[count]=*p;count++;return 1;
}
u16 sources_stable(void){u16 i;for(i=0;i<count;i++)if(*sources[i]!=values[i])return 0;return 1;}
''')
sources=['arcade_release.c','arcade_release_data.c','arcade_release_art.c','arcade_release_background.c']
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-DSGDK_GCC','-dynamiclib',
 '-I'+str(o),'-I'+str(r/'inc'),str(o/'shim.c')]+[str(r/'src'/s) for s in sources]+['-o',str(o/'test.dylib')],check=True)
lib=C.CDLL(str(o/'test.dylib'))
lib.arcade_release_background_step.argtypes=[C.c_uint16,C.c_uint8,C.c_uint8]
def scalar(n):return C.c_uint16.in_dll(lib,n)
addresses=(C.c_uint16*128).in_dll(lib,'addresses')
values=(C.c_uint16*128).in_dll(lib,'values')
maps=(C.c_uint16*32).in_dll(lib,'arcade_release_tiles')
checks=0
for phase in range(64):
 for camera in (0,7,8,239,240,247,248,255,256,3840,4095,8192,22271):
  lib.arcade_release_background_reset()
  assert (scalar('loaded_base').value,scalar('loaded_count').value)==(2032,14)
  lib.arcade_release_background_begin()
  lib.arcade_release_background_step(camera,1,phase)
  scalar('count').value=0
  lib.arcade_release_background_render();assert scalar('count').value==0
  for pose in range(8):
   lib.arcade_release_background_step(camera,5+pose*4,phase)
   scalar('capacity').value=23
   lib.arcade_release_background_render();assert scalar('count').value==0
   scalar('capacity').value=128;scalar('allowed').value=0
   lib.arcade_release_background_render();assert scalar('count').value==0
   scalar('allowed').value=1
   lib.arcade_release_background_render();assert scalar('count').value==24
   # Independently locate source column30 in the next32 world columns.
   world=next(x for x in range(camera//8,camera//8+32) if x%32==30)
   expected=[]
   for row in (28,24,20,16,12,8):
    expected.extend(0xe000+2*(y*64+(x+phase)%64)for x,y in
                    ((world,row),(world,row-1),(world+1,row),(world+1,row-1)))
   assert list(addresses)[:24]==expected
   assert list(values)[:24]==list(maps)[pose*4:pose*4+4]*6
   # Advancing to a nondrawing update must not change queued source words.
   lib.arcade_release_background_step(camera,6+pose*4,phase)
   assert lib.sources_stable()==1
   scalar('count').value=0
   lib.arcade_release_background_render();assert scalar('count').value==0
   checks+=24
  lib.arcade_release_background_step(camera,37,phase)
  lib.arcade_release_background_render();assert scalar('count').value==0
lib.arcade_release_background_reset();lib.arcade_release_background_begin()
lib.arcade_release_background_step(3840,0,0)
lib.arcade_release_background_step(3840,1,0)
lib.arcade_release_background_render()
lib.arcade_release_background_step(3848,5,0)
assert lib.sources_stable()==1  # pending pose changed; queued pointers did not
rom=b''.join((r/'arcade'/f).read_bytes() for f in ('ss.01e','ss.02e','ss.03e'))
g=G.Roms(str(r/'arcade'))
world=(r/'src/ss_level.c').read_text().split('world_tiles',1)[1].split('};',1)[0]
words=[int(v,16) for v in re.findall(r'0x([0-9a-fA-F]{8})',world)]
base=int(re.search(r'#define SS_LEVEL_TILE_BASE\s+(\d+)',(r/'inc/ss_level.h').read_text())[1])
pixels=(C.c_uint32*(14*8)).in_dll(lib,'arcade_release_pixels')
for pose in range(8):
 pattern=rom[0x68e2+pose*8:0x68ea+pose*8]
 for k in range(4):
  tile=maps[pose*4+k];a=pattern[k+4]
  actual=list(pixels)[(tile-2032)*8:(tile-2032+1)*8] if tile>=2032 else words[(tile-base)*8:(tile-base+1)*8]
  expected=G.bake_char_tile(g,pattern[k]+((a&3)<<8),a>>2)
  assert actual==list(expected),(pose,k,tile)
report=dict(map_words_checked=checks,exact_pixel_cells=32,plane_phases=64,camera_cases=13,poses=8,
 scope='Production renderer host test: ring mapping, queue admission/retry, immutable DMA sources, reset and final pose retirement. Live target visual parity remains unverified.')
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
