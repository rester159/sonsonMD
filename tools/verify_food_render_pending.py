#!/usr/bin/env python3
"""Differential pending-write renderer, including partial DMA and barrier refusal."""
from pathlib import Path
import subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/food-render-pending';o.mkdir(parents=True,exist_ok=True)
def render(path,name):
 s=path.read_text();a=s.index('void arcade_food_world_render(');b=s.index('void arcade_food_world_stop(',a)
 return s[a:b].replace('arcade_food_world_render',name)
s=(r/'src/arcade_food_world.c').read_text();a=s.index('static const u16 barrier_maps');b=s.index('static u16 screen;',a)
c='''#include <assert.h>
#include <string.h>
#include "arcade_world_object_pool.h"
#include "arcade_food_art.h"
#include "arcade_barrier_upload.h"
#define DMA_VRAM 0
static ArcadeWorldObjectPool pool;
static u8 writes[20],eaten[20],barrier_writes[20],barrier_pose[20],render_pending;
static u16 queued,budget,fail_at,attempts,log_count;static u8 refuse;
static u16 logwords[160][2];
static u16 DMA_getQueueSize(void){return queued;}
static u16 DMA_getMaxQueueSize(void){return 160;}
static u8 DMA_canQueue(u8 loc,u16 words){assert(!loc);return words<=budget;}
static u16 VDP_getBGAAddress(void){return 0xc000;}
static u8 DMA_queueDma(u8 loc,void*src,u16 address,u16 words,u16 step){
 assert(!loc&&words==1&&step==2);if(attempts++==fail_at)return 0;
 assert(log_count<160);logwords[log_count][0]=address;logwords[log_count++][1]=*(u16*)src;queued++;return 1;
}
static void DMA_doDma(u8 loc,void*src,u16 address,u16 words,u16 step){assert(DMA_queueDma(loc,src,address,words,step));}
u8 arcade_barrier_upload(u8 slot,u8 pose,u16 bytes,u16 ops){assert(slot<12);(void)pose;assert(bytes==520&&ops==8);return !refuse;}
'''+s[a:b]+render(r/'tools/fixtures/food_world_before_dirty_gate.c','before')+render(r/'src/arcade_food_world.c','after')+'''
int main(void){unsigned trial,k;u32 seed=1337;
for(trial=0;trial<20000;trial++){
 u8 iw[20],ib[20],wantw[20],wantb[20],pending=0;u16 wantlog[160][2],wantcount;
 for(k=0;k<20;k++){
  seed=seed*1664525u+1013904223u;
  pool.objects[k].x=seed;pool.objects[k].y=seed>>16;pool.objects[k].food=(u8)((seed%20)-12);
  eaten[k]=(seed>>8)&1;barrier_pose[k]=(seed>>24)%180;
  iw[k]=(trial%3)?((seed>>10)&3):0;ib[k]=(trial%3)?((seed>>12)&3):0;
  pending|=iw[k]|ib[k];
 }
 budget=(trial%4)?260:0;refuse=(trial%5==0);fail_at=(trial%7)?65535:trial%80;
 memcpy(writes,iw,20);memcpy(barrier_writes,ib,20);queued=attempts=log_count=0;memset(logwords,0,sizeof logwords);
 before(trial&63,0);memcpy(wantw,writes,20);memcpy(wantb,barrier_writes,20);memcpy(wantlog,logwords,sizeof logwords);wantcount=log_count;
 memcpy(writes,iw,20);memcpy(barrier_writes,ib,20);queued=attempts=log_count=0;memset(logwords,0,sizeof logwords);render_pending=!!pending;
 after(trial&63,0);
 assert(!memcmp(writes,wantw,20)&&!memcmp(barrier_writes,wantb,20));
 assert(log_count==wantcount&&!memcmp(logwords,wantlog,sizeof logwords));
 pending=0;for(k=0;k<20;k++)pending|=writes[k]|barrier_writes[k];assert(render_pending==!!pending);
 /* Retry retained writes with all refusals removed, then idle must issue none. */
 budget=260;refuse=0;fail_at=65535;queued=attempts=log_count=0;
 after(trial&63,0);assert(!render_pending);
 for(k=0;k<20;k++)assert(!writes[k]&&!barrier_writes[k]);
 queued=attempts=log_count=0;after(trial&63,0);assert(!attempts&&!log_count);
}
return 0;}
'''
(o/'test.c').write_text(c)
subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),str(r/'src/arcade_food_art.c'),str(r/'src/arcade_barrier_pose_data.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'render_retry_idle_sequences':20000,'scope':'Actual old/new render functions, identical write order/values/remaining masks under queue and barrier refusal plus partial transfer failure, eventual retry and idle. Host DMA shim; no target DMA timing claim.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
