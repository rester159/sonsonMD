#!/usr/bin/env python3
"""Offline9094 typed-player contract; other source fields remain caller-owned."""
from pathlib import Path
import ctypes as C
import json,subprocess,itertools
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/player-course';o.mkdir(parents=True,exist_ok=True)
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_player_course.c'),'-o',str(o/'test.dylib')],check=True)
lib=C.CDLL(str(o/'test.dylib'))
class Player(C.Structure):
 _fields_=[('x',C.c_uint16)]+[(n,C.c_uint8)for n in ('xf','y','yf','state','phase','timer','stamp','facing','previous','active','attached')]
class Animation(C.Structure):
 _fields_=[(n,C.c_uint8)for n in ('owner','cursor','timer','code','attr')]
lib.arcade_players_course_begin.argtypes=[C.POINTER(Player),C.POINTER(Animation),C.c_uint16,C.c_uint8,C.POINTER(C.c_uint8)]
checks=0
for camera,two,out0,out1 in itertools.product((0,1,224,3840,65503,65504,65535),(0,1,2,255),(0,1,128),(0,1,128)):
 p=(Player*2)();a=(Animation*2)()
 for i in range(2):
  for n,_ in Player._fields_:setattr(p[i],n,91+i)
  for n,_ in Animation._fields_:setattr(a[i],n,77+i)
 before=[{n:getattr(v,n)for n,_ in Player._fields_}for v in p]
 abefore=[{n:getattr(v,n)for n,_ in Animation._fields_}for v in a]
 lib.arcade_players_course_begin(p,a,camera,two,(C.c_uint8*2)(out0,out1))
 for i,out in enumerate((out0,out1)):
  expected=before[i].copy();anim=abefore[i].copy()
  if i==1:expected['active']=0
  if out==0 and (i==0 or two):
   expected.update(active=1,state=0,phase=0,x=(camera+32)%65536,y=104+i*32,facing=1)
   anim.update(owner=i,cursor=0,timer=0)
  assert {n:getattr(p[i],n)for n,_ in Player._fields_}==expected
  assert {n:getattr(a[i],n)for n,_ in Animation._fields_}==anim
 checks+=1
report={'cases':checks,'scope':'Offline inspected9094 typed fields: P2 clear order, eligibility, X wrap, idle-script reset and preservation of fractions/history/code/attributes. Does not cover source partner pointers, shot counters or caller game-flow binding.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
