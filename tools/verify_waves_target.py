#!/usr/bin/env python3
"""Replay original wave/entry contracts through the actual compiled 68000 ROM.
RAM JSR fixture with masked interrupts; this is ABI/arithmetic evidence, not play.
"""
from pathlib import Path
import argparse,hashlib,json,re,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limit',type=int,default=0);args=p.parse_args()
r=Path(__file__).resolve().parents[1];out=r/'out/parity-audit/target-waves';out.mkdir(parents=True,exist_ok=True)
rom_hash=hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest()
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
traces={
    'wave':(r/'out/parity-audit/wave-oracle.jsonl',r/'out/parity-audit/wave-comparison.json'),
    'entry':(r/'out/parity-audit/entry-oracle-v2.jsonl',r/'out/parity-audit/entry-comparison.json')
}
trace_hashes={}
for name,(trace,report)in traces.items():
    digest=hashlib.sha256(trace.read_bytes()).hexdigest()
    assert digest==json.loads(report.read_text())['trace_sha256'],('Unverified or overwritten source trace',name)
    trace_hashes[name]=digest
base=0xffb100;cases=[]
def word(v):return (v&65535).to_bytes(2,'big')
def control(b):return b[10:12]+bytes([b[0],b[2],b[3],b[1],b[14],0])
def actor(b):return b[5:7]+bytes([b[7],b[8],b[9],b[0],b[1],b[2],b[29],b[10]])
def case(fn,parameters,before,after,result=-1):cases.append('{'+','.join([str(syms[fn]),'{'+','.join(map(str,parameters))+'}',json.dumps(before.hex()),json.dumps(after.hex()),str(result)])+'}')
header=(r/'inc/ss_wave_sched.h').read_text();offsets=list(map(int,re.search(r'ss_wave_place_off[^=]+=[^{]*\{([^}]+)',header,re.S)[1].replace('\n','').strip(', ').split(',')))
program=b''.join((r/'arcade'/n).read_bytes()for n in ['ss.01e','ss.02e','ss.03e'])
def source_word(a):return int.from_bytes(program[a-0x4000:a-0x4000+2],'big')
last=255
for line in (r/'out/parity-audit/wave-oracle.jsonl').open():
 row=json.loads(line);kind=row['kind']
 if kind=='rng':continue
 if kind=='wave':
  wave=row['wave'];tick=row['tick']
  if tick==0:
   w=word(0)+word(0)+bytes([255,0,0,0,0,0])+b''.join(word(0x1234)+bytes([k%2,2,3,7,0xa5,0])for k in range(12))
  b=bytes.fromhex(row['producer']);source_ptr=int.from_bytes(b[4:6],'big');start=source_word(0xfcd9+wave*2)+2
  c=bytes.fromhex(row['controls']);after=b[6:8]+word(offsets[wave]+source_ptr-start)+bytes([b[0],b[2],(int.from_bytes(b[10:12],'big')-0x638)//16,b[8],0,0])+b''.join(control(c[k*16:k*16+16])for k in range(12))
  case('arcade_waves_step',[base,wave,0],w,after,65535 if tick==0 else tick-1);w=after
 elif kind=='delay':
  before=bytes(8)+bytes([row['delay'],0])+bytes(96);after=bytes(8)+bytes([row['after'],0])+bytes(96)
  case('arcade_waves_step',[base,0,row['clock']],before,after,65535)
 else:
  sp=row['statephase'];before=word(0x1234)+bytes([1,int(sp==3),0 if sp==3 else sp,1,row['parameter'],0])+word(row['seed'])+bytes([last,0])+bytes(36)+bytes([9])
  b=bytes.fromhex(row['actors']);count=6 if b[0] else 0
  born=b''.join(b[k*32+5:k*32+7]+bytes([b[k*32+8],b[k*32+29],b[k*32+10],0])for k in range(count)) if count else bytes(36)
  after=control(bytes.fromhex(row['control']))+word(row['next'])+bytes([row['last'],0])+born+bytes([row['reservation']])
  case('arcade_formation_step',[base,row['family'],row['live'],row['clock'],row['group']*16,0x2345,0x23f5,base+8,base+12,base+48],before,after,count);last=row['last']
for line in (r/'out/parity-audit/entry-oracle-v2.jsonl').open():
 row=json.loads(line)
 case('arcade_formation_enter',[base,row['family'],row['camera'],row['stopped']],actor(bytes.fromhex(row['before'])),actor(bytes.fromhex(row['after'])))
if args.limit:cases=cases[:args.limit]
s='local cases={\n'+',\n'.join(cases)+'\n}\n'+'''
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local index=0;local frame=0;local checks=0;local terminal=false
local function finish(error)
 if terminal then return end;terminal=true
 local f=assert(io.open(RESULT,"w"));f:write(string.format('{"cases":%d,"byte_checks":%d,"error":%s}',index,checks,error or "null"));f:close();manager.machine:exit()
end
local function puthex(addr,s)for k=1,#s,2 do m:write_u8(addr+(k-1)//2,tonumber(s:sub(k,k+1),16))end end
local function run(c)
 puthex(BASE,c[3])
 for a=0xffb000,0xffb03e,2 do m:write_u16(a,0x4e71)end
 m:write_u16(0xffb040,0x4eb9);m:write_u32(0xffb042,c[1]);m:write_u16(0xffb046,0x60fe)
 for i,v in ipairs(c[2])do m:write_u32(0xffbff0+(i-1)*4,v)end
 cpu.state["SR"].value=0x2700;cpu.state["SP"].value=0xffbff0;cpu.state["PC"].value=0xffb000
end
emu.register_frame_done(function()
 frame=frame+1
 if terminal then manager.machine:exit();return end
 if frame<60 then return end
 if index>0 then
  local c=cases[index]
  local pc=cpu.state["CURPC"].value&0xffffff
  if pc~=0xffb046 then finish(string.format('{"case":%d,"failure":"did not return","pc":%d}',index,pc));return end
  for k=1,#c[4],2 do
   local expected=tonumber(c[4]:sub(k,k+1),16);local actual=m:read_u8(BASE+(k-1)//2);checks=checks+1
   if actual~=expected then finish(string.format('{"case":%d,"byte":%d,"actual":%d,"expected":%d}',index,(k-1)//2,actual,expected));return end
  end
  local result_mask=c[1]==FORMATION_FUNCTION and 255 or 65535
  if c[5]>=0 and cpu.state["D0"].value&result_mask~=c[5]then finish(string.format('{"case":%d,"failure":"return value","actual":%d,"expected":%d}',index,cpu.state["D0"].value&result_mask,c[5]));return end
 end
 if index==#cases then finish();return end
 index=index+1;run(cases[index])
end)
'''
s=s.replace('FORMATION_FUNCTION',str(syms['arcade_formation_step'])).replace('BASE',str(base)).replace('RESULT',json.dumps(str(out/'results.json')));(out/'run.lua').write_text(s)
subprocess.run(['mame','genesis','-cart',str(r/'out/rom.bin'),'-window','-nomaximize','-resolution','512x448','-video','none','-keyboardprovider','none','-mouseprovider','none','-joystickprovider','none','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_script',str(out/'run.lua'),'-cfg_directory',str(out/'cfg'),'-nvram_directory',str(out/'nvram')],cwd=r,stdout=(out/'mame.log').open('w'),stderr=subprocess.STDOUT,check=True,timeout=240)
result=json.loads((out/'results.json').read_text());result['rom_sha256']=rom_hash;result['source_trace_sha256']=trace_hashes;result['scope']='Actual 68000 ROM functions, isolated RAM JSR fixture, not natural gameplay';(out/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));assert result['cases']==len(cases) and result['error']is None
assert hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest()==rom_hash

for name,(trace,_)in traces.items():assert hashlib.sha256(trace.read_bytes()).hexdigest()==trace_hashes[name]
