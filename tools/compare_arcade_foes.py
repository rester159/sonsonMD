#!/usr/bin/env python3
"""Compare native G/H combat, animation and straight-weapon state transitions."""
import ctypes as C,hashlib,json,pathlib,subprocess,collections
r=pathlib.Path(__file__).resolve().parents[1];o=r/'out/parity-audit'
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),*[str(r/'src'/n)for n in ['arcade_foes.c','arcade_foe_data.c','arcade_waves.c','arcade_formations.c']],'-o',str(o/'arcade_foes.dylib')],check=True)
class Body(C.Structure):_fields_=[('x',C.c_uint16)]+[(n,C.c_uint8)for n in ['xf','y','yf','active','state','phase','delay','facing']]
class Foe(C.Structure):_fields_=[('body',Body),('cursor',C.c_uint16),('pose',C.c_uint16)]+[(n,C.c_uint8)for n in ['kind','anim_timer','stamp','target','hit_style','death_timer']]
class Player(C.Structure):_fields_=[('x',C.c_uint16)]+[(n,C.c_uint8)for n in ['y','active','kind']]
class World(C.Structure):_fields_=[('camera',C.c_uint16)]+[(n,C.c_uint8)for n in ['clock','stage','tracking','scroll_stopped','shot_budget','purge']]+[('players',Player*2)]
class Weapon(C.Structure):_fields_=[(n,C.c_uint16)for n in ['x','origin','cursor','pose']]+[(n,C.c_uint8)for n in ['xf','y','yf','active','state','phase','facing','timer','anim_timer','parent']]
Floor=C.CFUNCTYPE(C.c_int16,C.c_uint16,C.c_uint8,C.c_void_p)
lib=C.CDLL(str(o/'arcade_foes.dylib'));lib.arcade_foe_step.argtypes=[C.POINTER(Foe),C.c_uint8,C.c_uint8,C.POINTER(World),Floor,C.c_void_p,C.POINTER(Weapon)];lib.arcade_foe_weapon_step.argtypes=[C.POINTER(Weapon),C.POINTER(World),C.POINTER(Foe)]
data=json.loads((o/'foe-extraction.json').read_text());frames=data['frames'];addresses={v['address']:k for k,v in enumerate(frames)};poses={(v['code'],v['attr']):k for k,v in enumerate(frames)}
def word(b,i):return int.from_bytes(b[i:i+2],'big')
def body(b):return Body(word(b,5),b[7],b[8],b[9],b[0],b[1],b[2],b[29],b[10])
def cursor(b):return addresses[word(b,14)]if word(b,14)else 0
def foe(b):return Foe(body(b),cursor(b),poses.get((b[16],b[17]),0),b[3],b[13],b[28],0 if not word(b,30)else 1+(word(b,30)==0x4c0),b[4],b[24])
def weapon(b):return Weapon(word(b,5),word(b,24),cursor(b),poses.get((b[16],b[17]),0),b[7],b[8],b[9],b[0],b[1],b[2],b[10],b[29],b[13],(word(b,30)-0xc28)//32 if word(b,30)else 0)
checks=0;rows=0;states=collections.Counter();weapon_states=collections.Counter();current={}
@Floor
def floor(x,y,unused):
 col=(((x+8-current['camera'])&65535)>>3)&31;bits=bytes.fromhex(current['collision']);row=y>>3;low=y&15
 if not bits[col*4+(row>>3)]&(128>>(row&7))or low<2:return -32767
 return 7-low
def check(a,b,name,row):
 global checks
 checks+=1
 assert a==b,{'field':name,'actual':a,'expected':b,'row':row}
def compare_pose(a,b,row):
 if word(b,14):
  check(frames[a.cursor]['address'],word(b,14),'animation_cursor',row)
  check(frames[a.pose]['code'],b[16],'sprite_code',row);check(frames[a.pose]['attr'],b[17],'sprite_attr',row)
  check(a.anim_timer,b[13],'animation_timer',row)
trace=o/'foe-oracle.jsonl'
for line in trace.open():
 row=json.loads(line);current=row;b=bytes.fromhex(row['before']);after=bytes.fromhex(row['after']);family=row['family'];slot=row['slot'];scenario=row['scenario']
 a=foe(b);p=bytes.fromhex(row['players']);players=(Player*2)(*[Player(word(p,k*48+5),p[k*48+8],p[k*48],p[k*48+3])for k in range(2)])
 w=World(row['camera'],row['tick']%256,24 if scenario==7 else 0,int(scenario==4),0,int(scenario!=7),0,players)
 wb=bytes.fromhex(row['weapons_before']);weapons=(Weapon*4)(*[weapon(wb[k*32:k*32+32])for k in range(4)])
 lib.arcade_foe_step(C.byref(a),family,slot,C.byref(w),floor,None,weapons)
 parents=(Foe*6)();parents[slot]=a
 for child in weapons:lib.arcade_foe_weapon_step(C.byref(child),C.byref(w),parents)
 expected=foe(after)
 for name,_ in Body._fields_:check(getattr(a.body,name),getattr(expected.body,name),name,row)
 for name in ['kind','anim_timer','stamp','target','hit_style','death_timer']:check(getattr(a,name),getattr(expected,name),name,row)
 compare_pose(a,after,row);states[f'{family}:{b[1]}:{b[2]}']+=1
 wa=bytes.fromhex(row['weapons_after'])
 for k,child in enumerate(weapons):
  raw=wa[k*32:k*32+32];ex=weapon(raw)
  for name,_ in Weapon._fields_:
   if name in ['cursor','pose']:continue
   check(getattr(child,name),getattr(ex,name),'weapon_'+str(k)+'_'+name,row)
  compare_pose(child,raw,row)
  if child.active:weapon_states[f'{child.state}:{child.phase}']+=1
 rows+=1
assert rows==19200,rows
result={'source':'Unmodified original MC6809 G/H and weapon dispatchers, isolated 400-tick trajectories','frames':rows,'field_checks':checks,'differences':0,'states':dict(states),'weapon_states':dict(weapon_states),'trace_sha256':hashlib.sha256(trace.read_bytes()).hexdigest()}
(o/'foe-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
