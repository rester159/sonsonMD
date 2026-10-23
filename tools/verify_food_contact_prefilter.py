#!/usr/bin/env python3
"""Differential test of live food-contact owner before/after geometry prefilter."""
from pathlib import Path
import ctypes as C
import json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/food-contact-prefilter';o.mkdir(parents=True,exist_ok=True)
s=(r/'src/arcade_food_world.c').read_text();a=s.index('u16 arcade_food_world_contact(');b=s.index('\nvoid arcade_food_world_render',a);native=s[a:b]
line='        if((u8)(y-o->y+6)>12 || (u16)(x-o->x+6)>12)continue;\n'
assert native.count(line)==1
reference=native.replace(line,'')
prefix='''#include <string.h>
#include "arcade_food.h"
#include "arcade_world_object_pool.h"
static ArcadeWorldObjectPool pool;static ArcadeFoodCounter counter;
static ArcadeSpecialSpawn arcade_food_spawn;static ArcadePowerup arcade_food_pickup;
'''
wrapper='''
u16 probe(u32 seed,u8*out){u16 i,n=0,points,x=(u16)seed,camera=(u16)(x-((seed>>16)&255));u8 y=(u8)(seed>>8);
 memset(&pool,0,sizeof(pool));memset(&arcade_food_spawn,0,sizeof(arcade_food_spawn));memset(&arcade_food_pickup,0,sizeof(arcade_food_pickup));
 counter.count=seed%6;counter.total=counter.count*((seed>>24)%10+1);
 for(i=0;i<20;i++){ArcadeWorldObject*p=&pool.objects[i];seed=seed*1664525u+1013904223u;
 p->x=(u16)(x+(s16)((seed>>8)%35)-17);p->y=(u8)(y+(s16)((seed>>16)%35)-17);
 p->active=seed&7;p->kind=(seed>>3)&3;p->phase=(seed>>5)&7;
 p->food=(u8)(244+(seed>>20)%24);p->timer=seed>>24;p->source_map=seed;
 }
 points=arcade_food_world_contact(camera,x,y,(seed&3)?1:2,(seed>>8)&1,(seed>>16)&15);
 out[n++]=points>>8;out[n++]=points;
 memcpy(out+n,&pool,sizeof(pool));n+=sizeof(pool);
 memcpy(out+n,&counter,sizeof(counter));n+=sizeof(counter);
 memcpy(out+n,&arcade_food_spawn,sizeof(arcade_food_spawn));n+=sizeof(arcade_food_spawn);
 memcpy(out+n,&arcade_food_pickup,sizeof(arcade_food_pickup));n+=sizeof(arcade_food_pickup);return n;
}
'''
libs=[]
for name,code in [('native',native),('reference',reference)]:
 path=o/(name+'.c');path.write_text(prefix+code+wrapper)
 subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(path),str(r/'src/arcade_food.c'),'-o',str(o/(name+'.dylib'))],check=True)
 lib=C.CDLL(str(o/(name+'.dylib')));lib.probe.argtypes=[C.c_uint32,C.POINTER(C.c_uint8)];lib.probe.restype=C.c_uint16;libs.append(lib)
bytes_checked=0
for seed in range(50000):
 buffers=[(C.c_uint8*512)()for _ in libs]
 counts=[lib.probe(seed*31337,buf)for lib,buf in zip(libs,buffers)]
 assert counts[0]==counts[1] and bytes(buffers[0])[:counts[0]]==bytes(buffers[1])[:counts[1]],seed
 bytes_checked+=counts[0]
report={'cases':50000,'bytes_compared':bytes_checked,'scope':'Production food-contact owner with/without broad-phase geometry; mixed types/active flags, wrapped boundaries, multi-hit slot order, six-food aggregation and pickup requests. Host equivalence only, not target speed measurement.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
