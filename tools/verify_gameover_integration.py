#!/usr/bin/env python3
"""Compare original A98A final-life cleanup with linked full game_update."""
from pathlib import Path
import argparse,hashlib,json,re,struct,subprocess
p=argparse.ArgumentParser();p.add_argument('--baseline',type=Path);p.add_argument('--expect-failure',action='store_true');args=p.parse_args()
r=Path(__file__).resolve().parents[1];build=args.baseline or r/'out';o=r/'out/parity-audit'/('gameover-baseline'if args.baseline else'gameover-integration');o.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
trace=r/'out/parity-audit/gameover-oracle.jsonl';rows=[json.loads(s)for s in trace.read_text().splitlines()]
assert len(rows)==96 and 'GAMEOVER_DONE'in(r/'out/parity-audit/gameover-source/mame.log').read_text()
assert hashlib.sha256(b''.join((r/'arcade'/n).read_bytes()for n in('ss.01e','ss.02e','ss.03e'))).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(build/'symbol.txt').read_text(),re.M)}
rh=sha(build/'rom.bin');sh=sha(build/'symbol.txt')
def convert(raw):
 b=bytes.fromhex(raw);owner=b[4];cursor=(int.from_bytes(b[14:16],'big')-(0xafac+owner*36))//3
 life=b[5:10]+bytes((b[0],b[1],b[2],b[3],owner,b[10],b[26],0,8+owner*4+cursor,b[13],b[16],b[17]))
 motion=b[5:10]+bytes((b[1],b[2],b[26],b[26],b[10],b[24]&60,b[0],b[29]))
 anim=bytes((owner,cursor,b[13],b[16],b[17]))
 return life.hex(),motion.hex(),anim.hex()
cases=[]
for d in rows:
 before=convert(d['before']);after=convert(d['after'])
 cases.append('{'+','.join([str(d['owner']),str(d['camera']),*[json.dumps(s)for s in before+after],str(d["teammate"])])+'}')
s='local cases={'+','.join(cases)+'}\n'+'''
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local frame=0;local index=0;local terminal=false;local current=nil
local function clear(a,n)for k=0,n-1 do m:write_u8(a+k,0)end end
local function bytes(a,s)for k=0,#s//2-1 do m:write_u8(a+k,tonumber(s:sub(k*2+1,k*2+2),16))end end
local function compare(a,s)for k=0,#s//2-1 do assert(m:read_u8(a+k)==tonumber(s:sub(k*2+1,k*2+2),16),'case '..index..' byte '..k..' address '..a)end end
local function finish(err)
 terminal=true;local f=assert(io.open(RESULT,'w'));f:write(string.format('{"cases":%d,"error":%s}',index,err and string.format('%q',err)or'null'));f:close();manager.machine:exit()
end
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if current then
   assert(cpu.state['CURPC'].value&0xffffff==0xffb046,'return')
   local k=current[1];compare(LIFE+k*18,current[6]);compare(MOTION+k*14,current[7]);compare(ANIM+k*5,current[8])
   local target=WORLD+8+k*6
   assert(m:read_u8(target+3)==tonumber(current[6]:sub(11,12),16),'target active')
   assert(m:read_u8(target+4)==0,'target kind')
   assert(m:read_u8(GG+k*42+34)==3,'game-over flow')
   index=index+1
  end
  if index==#cases then finish();return end
  local d=cases[index+1];current=d;local k=d[1]
  clear(GG,94);clear(OWN,2);clear(MOTION,28);clear(ANIM,10);clear(ENT,48*42);clear(WAVES,106);clear(FOES,240);clear(WEAPONS,72);clear(IA,96);clear(IW,10);clear(JOY,16)
  m:write_u8(ATTRACT,0);m:write_u8(SURVIVAL,0);m:write_u16(RIBBON,0);m:write_u8(GG+84,2);m:write_u16(GG+88,d[2])
  m:write_u8(GG+42+16,1);m:write_u8(GG+34,3);m:write_u8(GG+42+34,3);m:write_u8(GG+k*42+16,k)
  if d[9]==1 then
   local other=1-k;local q=GG+other*42
   m:write_u8(q+34,0);m:write_u8(q+32,3);m:write_u16(q,(d[2]+100)&65535);m:write_u16(q+2,135)
   m:write_u8(ANIM+other*5,other);m:write_u8(ANIM+other*5+4,192+other)
  end
  bytes(LIFE+k*18,d[3]);bytes(MOTION+k*14,d[4]);bytes(ANIM+k*5,d[5]);m:write_u8(OWN+k,2)
  local p=GG+k*42;local l=LIFE+k*18
  m:write_u16(p,m:read_u16(l));m:write_u16(p+4,m:read_u8(l+2));m:write_u16(p+2,(239-m:read_u8(l+3))&65535);m:write_u16(p+6,m:read_u8(l+4))
  for x=0xffb000,0xffb03e,2 do m:write_u16(x,0x4e71)end
  m:write_u16(0xffb040,0x4eb9);m:write_u32(0xffb042,FN);m:write_u16(0xffb046,0x60fe)
  cpu.state['SR'].value=0x2700;cpu.state['SP'].value=0xffbfe0;cpu.state['PC'].value=0xffb000
 end)
 if not ok then finish(err)end
end)
'''
for k,n in {'WORLD':'arcade_world','GG':'g_game','LIFE':'player_life','OWN':'life_owned','MOTION':'player_motion','ANIM':'player_animation','ENT':'g_entities','WAVES':'arcade_waves','FOES':'arcade_foes','WEAPONS':'arcade_weapons','IA':'arcade_i_actors','IW':'arcade_i_wave','JOY':'joyState','ATTRACT':'g_attract','SURVIVAL':'g_survivalRun','RIBBON':'g_ribbonW','FN':'game_update'}.items():s=re.sub(r'\b'+k+r'\b',str(syms[n]),s)
s=s.replace('RESULT',json.dumps(str(o/'results.json')));(o/'run.lua').write_text(s);(o/'results.json').unlink(missing_ok=True)
with (o/'mame.log').open('w')as f:subprocess.run(['mame','genesis','-cart',str(build/'rom.bin'),'-window','-nomaximize','-resolution','512x448','-video','none','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_delay','0','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],stdout=f,stderr=subprocess.STDOUT,check=True,timeout=90)
assert sha(build/'rom.bin')==rh and sha(build/'symbol.txt')==sh
result=json.loads((o/'results.json').read_text());result.update(rom_sha256=rh,trace_sha256=sha(trace),scope='96 injected post-game-over states through full game_update, idle input, teammate inactive/alive, source A98A owner asymmetry; natural final-death reachability separate')
(o/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(result)
assert (result['error']is not None)==args.expect_failure
if not args.expect_failure:assert result['cases']==96
