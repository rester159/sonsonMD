#!/usr/bin/env python3
"""Replay source G/H and weapon transitions through actual 68000 ROM functions.
A RAM leaf callback checks floor query arguments observed on the original CPU;
its returned push-out comes from that fixture's original collision bitmap.
"""
from pathlib import Path
import argparse,hashlib,json,re,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limit',type=int,default=0);args=p.parse_args()
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/target-foes';o.mkdir(parents=True,exist_ok=True)
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
d=json.loads((r/'out/parity-audit/foe-extraction.json').read_text());frames=d['frames'];addresses={v['address']:k for k,v in enumerate(frames)};poses={(v['code'],v['attr']):k for k,v in enumerate(frames)}
PARENTS=0xffb200;WEAPONS=0xffb300;WORLD=0xffb400;FLOOR=0xffb800;META=0xffb900
word=lambda v:(v&65535).to_bytes(2,'big');long=lambda v:(v&0xffffffff).to_bytes(4,'big')
def rd(b,i):return int.from_bytes(b[i:i+2],'big')
def animation(b):return word(addresses[rd(b,14)]if rd(b,14)else 0)+word(poses.get((b[16],b[17]),0))
def foe(b):return b[5:7]+bytes([b[7],b[8],b[9],b[0],b[1],b[2],b[29],b[10]])+animation(b)+bytes([b[3],b[13],b[28],0 if not rd(b,30)else 1+(rd(b,30)==0x4c0),b[4],b[24]])
def weapon(b):return b[5:7]+b[24:26]+animation(b)+bytes([b[7],b[8],b[9],b[0],b[1],b[2],b[10],b[29],b[13],(rd(b,30)-0xc28)//32 if rd(b,30)else 0])
def wrapper(calls):
 code=b'\x4e\x71'*16
 for fn,arguments in calls:
  for value in reversed(arguments):code+=b'\x2f\x3c'+long(value)
  code+=b'\x4e\xb9'+long(fn)+b'\x4f\xef'+word(len(arguments)*4)
 return code+b'\x60\xfe'
def leaf(result):return b'\x30\x2f\x00\x06\x33\xc0'+long(META+2)+b'\x10\x2f\x00\x0b\x13\xc0'+long(META+4)+b'\x52\x79'+long(META)+b'\x30\x3c'+word(result)+b'\x4e\x75'
trace=r/'out/parity-audit/foe-oracle.jsonl'
trace_hash=hashlib.sha256(trace.read_bytes()).hexdigest()
assert trace_hash==json.loads((r/'out/parity-audit/foe-comparison.json').read_text())['trace_sha256'],'Unverified or overwritten source trace'
cases=[]
for index,line in enumerate((r/'out/parity-audit/foe-oracle.jsonl').open()):
 if args.limit and index>=args.limit:break
 row=json.loads(line);b=bytes.fromhex(row['before']);a=bytes.fromhex(row['after']);wb=bytes.fromhex(row['weapons_before']);wa=bytes.fromhex(row['weapons_after']);ps=bytes.fromhex(row['players']);slot=row['slot'];scenario=row['scenario']
 parents=bytes(slot*20)+foe(b)+bytes((5-slot)*20)
 world=word(row['camera'])+bytes([row['tick']%256,24 if scenario==7 else 0,int(scenario==4),0,int(scenario!=7),0])+b''.join(ps[k*48+5:k*48+7]+bytes([ps[k*48+8],ps[k*48],ps[k*48+3],0])for k in range(2))
 before_weapons=b''.join(weapon(wb[k*32:k*32+32])for k in range(4));after_weapons=b''.join(weapon(wa[k*32:k*32+32])for k in range(4))
 result=-32767
 if row['floor_x']>=0:
  x,y=row['floor_x'],row['floor_y'];col=(((x+8-row['camera'])&65535)>>3)&31;bits=bytes.fromhex(row['collision']);line=y>>3;low=y&15
  if low>=2 and bits[col*4+(line>>3)]&(128>>(line&7)):result=7-low
 if 'arcade_foe_step_g' in syms and 'arcade_foe_step_h' in syms:
  # Exercise the production fixed-family entry, including its actual ABI.
  actor_call=(syms['arcade_foe_step_h'],[PARENTS+slot*20,slot,WORLD,FLOOR,0,WEAPONS]) if row['family'] else (syms['arcade_foe_step_g'],[PARENTS+slot*20,slot,WORLD,FLOOR,0,WEAPONS,0])
 else:
  actor_call=(syms['arcade_foe_step'],[PARENTS+slot*20,row['family'],slot,WORLD,FLOOR,0,WEAPONS])
 calls=[actor_call]+[(syms['arcade_foe_weapon_step'],[WEAPONS+k*18,WORLD,PARENTS])for k in range(4)]
 code=wrapper(calls)
 expected_poses=[[a[16],a[17],int(rd(a,14)!=0)]]+[[wa[k*32+16],wa[k*32+17],int(rd(wa[k*32:k*32+32],14)!=0)]for k in range(4)]
 values=[code.hex(),parents.hex(),before_weapons.hex(),world.hex(),leaf(result).hex(),foe(a).hex(),after_weapons.hex(),slot,row['floor_x'],row['floor_y'],len(code)-2,expected_poses]
 cases.append('{'+','.join('{'+','.join('{'+','.join(map(str,t))+'}'for t in v)+'}'if isinstance(v,list)else json.dumps(v)for v in values)+'}')
s='local cases={\n'+',\n'.join(cases)+'\n}\n'+'''
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local frame=0;local index=0;local checks=0;local terminal=false
local function finish(error)
 if terminal then return end;terminal=true
 local f=assert(io.open(RESULT,"w"));f:write(string.format('{"cases":%d,"checks":%d,"error":%s}',index,checks,error or "null"));f:close();manager.machine:exit()
end
local function put(a,s)for k=1,#s,2 do m:write_u8(a+(k-1)//2,tonumber(s:sub(k,k+1),16))end end
local function compare(a,s,stride,pose_offset)
 for k=1,#s,2 do
  local offset=(k-1)//2
  if offset%stride~=pose_offset and offset%stride~=pose_offset+1 then
   checks=checks+1;local actual=m:read_u8(a+offset);local expected=tonumber(s:sub(k,k+1),16)
   if actual~=expected then finish(string.format('{"case":%d,"address":%d,"offset":%d,"actual":%d,"expected":%d}',index,a,offset,actual,expected));return false end
  end
 end
 return true
end
emu.register_frame_done(function()
 frame=frame+1;if terminal then manager.machine:exit();return end;if frame<60 then return end
 if index>0 then
  local c=cases[index]
  if cpu.state["CURPC"].value&0xffffff~=0xffb000+c[11]then finish('{"failure":"routine did not return"}');return end
  if not compare(PARENTS+c[8]*20,c[6],20,12)or not compare(WEAPONS,c[7],18,6)then return end
  local count=m:read_u16(META);checks=checks+1
  if count~=(c[9]<0 and 0 or 1)then finish(string.format('{"case":%d,"floor_calls":%d}',index,count));return end
  if count>0 then
   checks=checks+2
   if m:read_u16(META+2)~=c[9]or m:read_u8(META+4)~=c[10]then finish(string.format('{"case":%d,"floor_x":%d,"floor_y":%d}',index,m:read_u16(META+2),m:read_u8(META+4)));return end
  end
  for k,expected in ipairs(c[12])do
   if expected[3]~=0 then
    local pose=m:read_u16(k==1 and PARENTS+c[8]*20+12 or WEAPONS+(k-2)*18+6);checks=checks+2
    if m:read_u8(FRAMES+pose*8+4)~=expected[1]or m:read_u8(FRAMES+pose*8+5)~=expected[2]then finish(string.format('{"case":%d,"pose_owner":%d,"pose":%d}',index,k,pose));return end
   end
  end
 end
 if index==#cases then finish();return end
 index=index+1;local c=cases[index]
 put(0xffb000,c[1]);put(PARENTS,c[2]);put(WEAPONS,c[3]);put(WORLD,c[4]);put(FLOOR,c[5]);m:write_u16(META,0)
 cpu.state["SR"].value=0x2700;cpu.state["SP"].value=0xffbff0;cpu.state["PC"].value=0xffb000
end)
'''
for k,v in {'RESULT':json.dumps(str(o/'results.json')),'PARENTS':PARENTS,'WEAPONS':WEAPONS,'WORLD':WORLD,'FLOOR':FLOOR,'META':META,'FRAMES':syms['arcade_foe_frames']}.items():s=re.sub(r'\b'+k+r'\b',str(v),s)
(o/'run.lua').write_text(s);identity=hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest()
subprocess.run(['mame','genesis','-cart',str(r/'out/rom.bin'),'-window','-nomaximize','-resolution','512x448','-video','none','-keyboardprovider','none','-mouseprovider','none','-joystickprovider','none','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],cwd=r,stdout=(o/'mame.log').open('w'),stderr=subprocess.STDOUT,check=True,timeout=300)
result=json.loads((o/'results.json').read_text());result['rom_sha256']=identity;result['source_trace_sha256']=trace_hash;result['scope']='Compiled 68000 G/H and weapon routines, original-CPU floor query arguments and collision results via RAM callback fixture';(o/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));assert result['cases']==len(cases)and result['error']is None
assert hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest()==identity

assert hashlib.sha256(trace.read_bytes()).hexdigest()==trace_hash
