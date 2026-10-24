#!/usr/bin/env python3
"""Compare optimized live food owner with its prefilter-free reference."""
from pathlib import Path
import ctypes as C,json,subprocess,hashlib,random,argparse
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/food-world-equivalence';o.mkdir(parents=True,exist_ok=True)
parser=argparse.ArgumentParser();parser.add_argument('--source',type=Path,default=r/'src/arcade_food_world.c');args=parser.parse_args()
source=args.source.read_text()
reference=(r/'tools/fixtures/food_world_before_dirty_gate.c').read_text().replace('        if(o->phase==0 && (!o->y || edge<=o->x))continue;\n','').replace('        if((o->phase==1 || o->phase==2) && !advance)continue;\n','').replace('u16 i,edge=(u16)(camera+256);ArcadeFoodDraw draw;','u16 i;ArcadeFoodDraw draw;').replace('    if(active!=1)return 0;\n','').replace('        if(!(o->active&1) || o->kind || (u16)(o->x-camera)>=256)continue;\n','').replace('        if(!o->active || o->kind || o->phase==3 || o->phase==5)continue;\n','')
(o/'genesis.h').write_text('''#include "arcade_types.h"
#include <string.h>
#define DMA_VRAM 0
static unsigned short vram[32768];
static unsigned short queued;
static unsigned short DMA_getQueueSize(void){return queued;}
static unsigned short DMA_getMaxQueueSize(void){return 80;}
static unsigned char DMA_canQueue(unsigned char l,unsigned short w){(void)l;return queued+w<=80;}
static unsigned short VDP_getBGAAddress(void){return 0xe000;}
static void DMA_doDma(unsigned char l,void *p,unsigned short a,unsigned short n,short step){(void)l;while(n--){vram[a/2]=*(unsigned short*)p;p=(unsigned short*)p+1;a+=step;}}
static unsigned char DMA_queueDma(unsigned char l,void *p,unsigned short a,unsigned short n,unsigned short step){if(queued+n>80)return 0;queued+=n;DMA_doDma(l,p,a,n,step);return 1;}
/* Production map words use the VBlank word list; same capacity model. Host
 * runs never count as a late frame (no barrier deferral). */
#define GET_VCOUNTER 0
static inline unsigned char vram_words_room(unsigned short n){return queued+n<=80;}
static inline unsigned char vram_words_queue(const unsigned short *p,unsigned short a,unsigned short n,unsigned short step){return DMA_queueDma(0,(void*)p,a,n,step);}
#define VRAM_WORDS_H
''')
wrapper='''
/* Host food equivalence deliberately rejects barrier DMA; target tests own it. */
void arcade_barrier_upload_reset(void){}
unsigned char arcade_barrier_upload(unsigned char slot,unsigned char pose,unsigned short bytes,unsigned short ops){(void)slot;(void)pose;(void)bytes;(void)ops;return 0;}
unsigned short snapshot(unsigned char *out){unsigned short n=0,i;
#define B(v) out[n++]=(unsigned char)(v)
#define W(v) B((v)>>8);B(v)
for(i=0;i<20;i++){ArcadeWorldObject *a=&pool.objects[i];W(a->x);W(a->source_map);B(a->active);B(a->phase);B(a->y);B(a->food);B(a->timer);B(a->kind);B(writes[i]);B(eaten[i]);B(barrier_cursor[i]);B(barrier_writes[i]);B(barrier_pose[i]);}
B(pool.block);B(pool.quarter);B(counter.count);B(counter.total);W(screen);
B(arcade_food_spawn.active);B(arcade_food_spawn.phase);B(arcade_food_spawn.placement);B(arcade_food_spawn.row);B(arcade_food_pickup.kind);B(arcade_food_pickup.exit_latched);
return n;}
void render(unsigned char phase,unsigned short budget){queued=80-budget;arcade_food_world_render(phase,0);}
unsigned short tile(unsigned short a){return vram[a];}
'''
libs=[]
for name,text in [('native',source),('reference',reference)]:
 p=o/(name+'.c');body=wrapper
 if name=='native':body=body.replace('unsigned short n=0,i;', 'unsigned short n=0,i;for(i=0;i<20;i++)assert(render_pending || (!writes[i] && !barrier_writes[i]));')
 p.write_text('#include <assert.h>\n'+text+body);lib=o/(name+'.dylib')
 subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(o),'-I'+str(r/'inc'),str(p),*[str(r/'src'/f)for f in ['arcade_world_objects.c','arcade_world_object_pool.c','arcade_food_step.c','arcade_food_patterns.c','arcade_food.c','arcade_food_art.c','arcade_barrier.c','arcade_barrier_step.c','arcade_barrier_data.c','arcade_barrier_pose_data.c','arcade_stop.c']],'-o',str(lib)],check=True)
 l=C.CDLL(str(lib));l.arcade_food_world_contact.argtypes=[C.c_uint16,C.c_uint16,*([C.c_uint8]*4)];l.arcade_food_world_contact.restype=C.c_uint16;l.snapshot.argtypes=[C.POINTER(C.c_uint8)];libs.append(l)
rng=random.Random(93417);snap=(C.c_uint8*400)();checks=contacts=0
for players in (1,2):
 for l in libs:l.arcade_food_world_begin()
 for tick in range(40000):
  camera=tick*141//256;advance=int((camera&7)==0)
  for l in libs:l.arcade_food_world_step(camera,advance)
  for owner in range(players):
   n=libs[0].snapshot(snap);i=rng.randrange(20)*12;x=(snap[i]<<8)|snap[i+1];y=snap[i+6]
   args=(camera,(x+rng.randrange(-7,8))&65535,(y+rng.randrange(-7,8))&255,rng.choice([0,1,1,1,2]),owner,tick//10000)
   awards=[l.arcade_food_world_contact(*args)for l in libs];assert awards[0]==awards[1];contacts+=bool(awards[0])
  budget=rng.choice([0,1,2,3,4,80]);phase=(tick//8192*32)&63
  for l in libs:l.render(phase,budget)
  states=[]
  for l in libs:n=l.snapshot(snap);states.append(bytes(snap[:n]))
  assert states[0]==states[1],(players,tick)
  checks+=1
 for addr in range(0x7000,0x7800):assert libs[0].tile(addr)==libs[1].tile(addr)
report=dict(updates=checks,successful_contacts=contacts,source_sha256=hashlib.sha256(source.encode()).hexdigest(),scope='Optimized versus prefilter-free owner: deterministic1P/2P, dormant phases, pool recycling, coordinate/contact edges, queue refusals and final nametable. Host equivalence with barrier uploads explicitly rejected; actual barrier DMA is covered by target tests. Not independent whole-game arcade proof.')
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
