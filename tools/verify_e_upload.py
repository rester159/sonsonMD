#!/usr/bin/env python3
from pathlib import Path
import subprocess,json,ctypes as C
import gen_assets as G
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/e-upload';o.mkdir(parents=True,exist_ok=True)
(o/'genesis.h').write_text('''#include "arcade_types.h"
#define DMA_VRAM 0
u16 DMA_getQueueSize(void);u16 DMA_getMaxQueueSize(void);u16 DMA_getQueueTransferSize(void);
u8 DMA_canQueue(u8 location,u16 words);
u8 DMA_queueDma(u8 location,void *source,u16 destination,u16 words,u16 step);
''')
(o/'test.c').write_text('''#include <assert.h>
#include <string.h>
#include "genesis.h"
#include "arcade_e_upload.h"
static u16 count,bytes,capacity=4096,limit=40,calls;
static struct {void *source;u16 dest;} pending[4];
static unsigned char vram[65536];
u16 DMA_getQueueSize(void){return count;}
u16 DMA_getQueueTransferSize(void){return bytes;}
u16 DMA_getMaxQueueSize(void){return limit;}
u8 DMA_canQueue(u8 loc,u16 words){assert(!loc);return bytes+words*2<=capacity;}
u8 DMA_queueDma(u8 loc,void *src,u16 dest,u16 words,u16 step){
 assert(!loc&&words==64&&step==2&&count<limit&&bytes+128<=capacity);
 pending[calls].source=src;pending[calls].dest=dest;calls++;count++;bytes+=128;return 1;
}
static void flush(void){u16 k;for(k=0;k<calls;k++)memcpy(vram+pending[k].dest,pending[k].source,128);calls=count=bytes=0;}
int main(void){u16 f[2]={0,1},a,b,k;unsigned i;
 memset(vram,0xa5,sizeof vram);arcade_e_upload_reset();
 capacity=255;assert(!arcade_e_upload(f,0,0)&&!calls);
 capacity=4096;limit=1;assert(!arcade_e_upload(f,0,0)&&!calls);limit=40;
 f[1]=12;assert(!arcade_e_upload(f,0,0)&&!calls);f[1]=1;
 assert(!arcade_e_upload(f,65535,0)&&!calls);
 assert(arcade_e_upload(f,704,2)&&calls==2);flush();
 assert(arcade_e_upload(f,704,2)&&!calls);
 /* Full DMA queue must not hide matching resident art or empty requests. */
 capacity=0;limit=0;
 assert(arcade_e_upload(f,704,2)&&!calls);
 {u16 empty[2]={65535,65535};assert(arcade_e_upload(empty,704,2)&&!calls);}
 f[0]=12;assert(!arcade_e_upload(f,704,2)&&!calls);f[0]=0;
 f[1]=2;assert(!arcade_e_upload(f,704,2)&&!calls);f[1]=1;
 assert(arcade_e_upload(f,704,2)&&!calls);
 capacity=4096;limit=40;
 f[0]=2;capacity=831;assert(!arcade_e_upload(f,704,2)&&!calls);
 capacity=832;assert(arcade_e_upload(f,704,2)&&calls==1);flush();capacity=4096;
 for(a=0;a<12;a++)for(b=0;b<12;b++){
  f[0]=a;f[1]=b;assert(arcade_e_upload(f,704,2));flush();
  for(k=0;k<2;k++)assert(!memcmp(vram+arcade_e_upload_tiles[k]*32,arcade_e_pixels[f[k]],128));
  assert(arcade_e_upload(f,704,2)&&!calls);
 }
 f[0]=65535;assert(arcade_e_upload(f,0,0)&&!calls);f[0]=11;
 assert(arcade_e_upload(f,0,0)&&!calls);
 arcade_e_upload_reset();assert(arcade_e_upload(f,0,0)&&calls==2);flush();
 for(i=0;i<65536;i++)if(!(i>=0xf320&&i<0xf3a0)&&!(i>=0xf680&&i<0xf700))assert(vram[i]==0xa5);
 return 0;}
''')
old_command=['cc','-std=c99','-Wall','-Wextra','-Werror','-Wno-pointer-to-int-cast','-I'+str(o),'-I'+str(r/'inc'),str(o/'test.c'),str(r/'tools/fixtures/e_upload_before_noop.c'),str(r/'src/arcade_e_art.c'),'-o',str(o/'old-test')]
subprocess.run(old_command,check=True)
old=subprocess.run([str(o/'old-test')],capture_output=True)
assert old.returncode!=0 and b'arcade_e_upload(f,704,2)' in old.stderr
(o/'old-failure.txt').write_bytes(old.stderr)
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-Wno-pointer-to-int-cast','-fsanitize=address,undefined','-I'+str(o),'-I'+str(r/'inc'),str(o/'test.c'),str(r/'src/arcade_e_upload.c'),str(r/'src/arcade_e_art.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
subprocess.run(['cc','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_e_art.c'),'-o',str(o/'art.dylib')],check=True)
l=C.CDLL(str(o/'art.dylib'));l.arcade_e_art.argtypes=[C.c_uint8,C.c_uint8];l.arcade_e_art.restype=C.c_uint16
pixels=list((C.c_uint32*(12*32)).in_dll(l,'arcade_e_pixels'));roms=G.Roms(str(r/'arcade'))
for code in range(113,125):
 expected=list(G.bake_sprite_tiles(roms,code,4));index=code-113
 assert pixels[index*32:index*32+32]==expected
 for flip in (0,64,128,192):assert l.arcade_e_art(code,4|flip)==(0x8000|index)
assert l.arcade_e_art(255,31)==65535
report={'old_noop_regression_reproduced':True,'cache_pairs':144,'new_pixel_checks':12*256,'max_upload_bytes':256,'scope':'Sanitized host deferred-transfer/reuse/refusal/reservation/arena checks and compiled new art pixel comparisons. Live integration, target queue boundary splits, VRAM readback and performance pending.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
