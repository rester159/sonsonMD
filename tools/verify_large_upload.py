#!/usr/bin/env python3
"""Exercise upload admission/reuse and deferred immutable-source transfers."""
from pathlib import Path
import subprocess,json,hashlib
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/large-upload';o.mkdir(parents=True,exist_ok=True)
(o/'genesis.h').write_text('''#include "arcade_types.h"
#define DMA_VRAM 0
extern u16 font_index;
#define TILE_FONT_INDEX font_index
u16 DMA_getQueueSize(void);u16 DMA_getMaxQueueSize(void);u16 DMA_getQueueTransferSize(void);
u8 DMA_canQueue(u8 location,u16 words);
u8 DMA_queueDma(u8 location,void *source,u16 destination,u16 words,u16 step);
''')
(o/'test.c').write_text('''#include <assert.h>
#include <string.h>
#include "genesis.h"
#include "arcade_large_upload.h"
u16 font_index=1376;
static u16 count,bytes,capacity=4096,limit=40,calls;
static struct {void *source;u16 dest,words;} pending[32];
static unsigned char vram[65536];
u16 DMA_getQueueSize(void){return count;}
u16 DMA_getQueueTransferSize(void){return bytes;}
u16 DMA_getMaxQueueSize(void){return limit;}
u8 DMA_canQueue(u8 location,u16 words){assert(location==0);return bytes+words*2<=capacity;}
u8 DMA_queueDma(u8 location,void *source,u16 dest,u16 words,u16 step){
 assert(location==0 && step==2 && words==64);assert(count<limit && bytes+words*2<=capacity);
 pending[calls].source=source;pending[calls].dest=dest;pending[calls].words=words;calls++;count++;bytes+=words*2;return 1;
}
static void flush(void){u16 k;for(k=0;k<calls;k++)memcpy(vram+pending[k].dest,pending[k].source,pending[k].words*2);calls=count=bytes=0;}
int main(void){
 u16 f[16],k;memset(vram,0xa5,sizeof(vram));arcade_large_upload_reset();
 for(k=0;k<16;k++)f[k]=k;
 /* Refusal must queue nothing and must not mark an untransferred pose resident. */
 capacity=2047;assert(!arcade_large_upload(f,0,0));assert(calls==0);
 capacity=4096;limit=15;assert(!arcade_large_upload(f,0,0));assert(calls==0);
 limit=40;font_index=1371;assert(!arcade_large_upload(f,0,0));assert(calls==0);font_index=1376;
 f[15]=ARCADE_LARGE_ART_FRAMES;assert(!arcade_large_upload(f,0,0));assert(calls==0);f[15]=15;
 assert(!arcade_large_upload(f,65535,0));assert(calls==0);
 assert(arcade_large_upload(f,704,2));assert(calls==16 && bytes==2048);flush();
 for(k=0;k<16;k++)assert(!memcmp(vram+(1308+k*4)*32,arcade_large_art_pixels+k*32,128));
 for(k=0;k<1308*32;k++)assert(vram[k]==0xa5);
 for(k=1372*32;k<65535;k++)assert(vram[k]==0xa5);
 assert(arcade_large_upload(f,704,2));assert(calls==0);
 f[7]=150;capacity=831;assert(!arcade_large_upload(f,704,2));assert(calls==0);
 capacity=832;assert(arcade_large_upload(f,704,2));assert(calls==1 && bytes==128);flush();
 assert(!memcmp(vram+(1308+7*4)*32,arcade_large_art_pixels+150*32,128));
 f[7]=65535;assert(arcade_large_upload(f,0,0));assert(calls==0);
 f[7]=150;assert(arcade_large_upload(f,0,0));assert(calls==0);
 arcade_large_upload_reset();capacity=4096;assert(arcade_large_upload(f,0,0));assert(calls==16);flush();
 return 0;
}
''')
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-Wno-pointer-to-int-cast','-fsanitize=address,undefined','-I'+str(o),'-I'+str(r/'inc'),str(o/'test.c'),str(r/'src/arcade_large_upload.c'),str(r/'src/arcade_large_art.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
d={'status':'pass','source_sha256':hashlib.sha256((r/'src/arcade_large_upload.c').read_bytes()).hexdigest(),'arena_tiles':[1308,1372],'maximum_changed_bytes':2048,'scope':'Host mocked queue capacity, rejection/retry, dirty reuse, deferred transfer and arena boundaries under ASan/UBSan. Target VDP, split transfers, timing and gameplay integration pending.'}
(o/'results.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2))
