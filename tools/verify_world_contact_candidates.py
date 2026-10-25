#!/usr/bin/env python3
"""Compare optimized live food owner with its prefilter-free reference."""
from pathlib import Path
import ctypes as C,json,subprocess,hashlib,random,argparse
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/world-contact-candidates-check';o.mkdir(parents=True,exist_ok=True)
parser=argparse.ArgumentParser();parser.add_argument('--source',type=Path,default=r/'src/arcade_food_world.c');args=parser.parse_args()
source=args.source.read_text()
a=source.index('u16 arcade_food_world_shot(');b=source.index('u16 arcade_food_world_contact(',a)
reference=(r/'tools/fixtures/food_world_before_contact_candidates.c').read_text()
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
wrapper+='\nvoid shot_probe(unsigned int seed,unsigned char *out){\n unsigned short i,n=0,points;ArcadePlayerShot shot;\n shot.x=(unsigned short)seed;shot.y=(unsigned char)(seed>>16);shot.active=(seed&7)==0?2:1;shot.state=7;shot.phase=9;\n for(i=0;i<20;i++){\n  ArcadeWorldObject *a=&pool.objects[i];seed=seed*1664525u+1013904223u;\n  a->x=(unsigned short)(shot.x+(short)((seed>>8)%61)-30);\n  a->y=(unsigned char)(shot.y+(short)((seed>>16)%31)-15);\n  a->active=(unsigned char)(seed&7);a->kind=(seed&8)?2:0;a->phase=(seed>>4)&7;\n  barrier_cursor[i]=(seed>>24)%40;\n }\n points=arcade_food_world_shot(&shot);\n W(points);W(shot.x);B(shot.y);B(shot.active);B(shot.state);B(shot.phase);\n for(i=0;i<20;i++){W(pool.objects[i].x);B(pool.objects[i].y);B(pool.objects[i].active);B(pool.objects[i].kind);B(pool.objects[i].phase);B(barrier_cursor[i]);}\n}\n'
wrapper+='\nvoid player_probe(u16 x,u8 y,u8 active,u16 camera,u16 *out){ArcadePlayer p;memset(&p,0,sizeof p);p.x=x;p.y=y;p.active=active;p.state=7;p.phase=9;out[0]=arcade_food_world_player(&p,camera);out[1]=p.x;out[2]=p.state;out[3]=p.phase;}\n'
libs=[]
for name,text in [('native',source),('reference',reference)]:
 p=o/(name+'.c');p.write_text(text+wrapper.replace(' points=arcade_food_world_shot(&shot);',(' candidates_dirty=1;' if name=='native' else '')+' points=arcade_food_world_shot(&shot);'));lib=o/(name+'.dylib')
 subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(o),'-I'+str(r/'inc'),str(p),*[str((r/'src')/f)for f in ['arcade_world_objects.c','arcade_world_object_pool.c','arcade_food_step.c','arcade_food_patterns.c','arcade_food.c','arcade_food_art.c','arcade_barrier.c','arcade_barrier_step.c','arcade_barrier_data.c','arcade_barrier_pose_data.c','arcade_stop.c']],'-o',str(lib)],check=True)
 l=C.CDLL(str(lib));l.arcade_food_world_contact.argtypes=[C.c_uint16,C.c_uint16,*([C.c_uint8]*4)];l.arcade_food_world_contact.restype=C.c_uint16;l.snapshot.argtypes=[C.POINTER(C.c_uint8)];libs.append(l)
for lib in libs:lib.shot_probe.argtypes=[C.c_uint32,C.POINTER(C.c_uint8)]
for seed in range(20000):
 out=[(C.c_uint8*148)() for _ in libs]
 for lib,buf in zip(libs,out):lib.shot_probe(seed*31337,buf)
 assert bytes(out[0])==bytes(out[1]),seed

# Exercise real loaders and state changes, including same-tick player/shot order.
class Shot(C.Structure):
 _fields_=[('x',C.c_uint16),('origin',C.c_uint16)]+[(n,C.c_uint8)for n in ('xf','y','yf','active','state','phase','facing','timer','anim_timer','cursor','pose')]
for lib in libs:
 lib.arcade_food_world_begin()
 lib.player_probe.argtypes=[C.c_uint16,C.c_uint8,C.c_uint8,C.c_uint16,C.POINTER(C.c_uint16)]
 lib.arcade_food_world_step_clock.argtypes=[C.c_uint16,C.c_uint8,C.c_uint8]
 lib.arcade_food_world_shot.argtypes=[C.POINTER(Shot)]
 lib.arcade_food_world_shot.restype=C.c_uint16
samples=0
for tick in range(50000):
 camera=(tick*141//256)%22272
 if tick and camera<((tick-1)*141//256)%22272:
  for lib in libs:lib.arcade_food_world_begin()
 advance=(camera//8)!=(((tick-1)*141//256)%22272)//8
 for lib in libs:lib.arcade_food_world_step_clock(camera,advance,tick&255)
 # Near-object contacts trigger collections as objects enter the viewport.
 for slot in (tick%20,(tick+7)%20):
  buf=(C.c_uint8*512)();libs[0].snapshot(buf);off=slot*15
  x=(buf[off]<<8)|buf[off+1];y=buf[off+6]
  points=[lib.arcade_food_world_contact(camera,x,y,1,tick&1,2) for lib in libs]
  assert points[0]==points[1],('contact',tick,slot,points)
  pushes=[(C.c_uint16*4)() for _ in libs]
  for lib,push in zip(libs,pushes):lib.player_probe(x,y,1,camera,push)
  assert bytes(pushes[0])==bytes(pushes[1]),('push',tick,slot)
  shots=[Shot(x=x,y=y,active=1)for _ in libs]
  scores=[lib.arcade_food_world_shot(C.byref(shot))for lib,shot in zip(libs,shots)]
  assert scores[0]==scores[1] and bytes(shots[0])==bytes(shots[1]),('shot',tick,slot)
 out=[(C.c_uint8*512)() for _ in libs]
 sizes=[lib.snapshot(buf)for lib,buf in zip(libs,out)]
 assert sizes[0]==sizes[1] and bytes(out[0])==bytes(out[1]),('world',tick)
 samples+=1
report={'passed':True,'random_shot_cases':20000,'world_steps':samples,'player_push_comparisons':samples*2,'contact_and_shot_pairs':samples*2,'scope':'Real world loader, food/barrier step and contacts with cache invalidation across complete ribbon/restart; compares full pool, write masks, counters, pickup spawn, cursor and shot/award states. Host DMA shim, no target performance claim.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
