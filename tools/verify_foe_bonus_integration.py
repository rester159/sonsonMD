#!/usr/bin/env python3
"""Live entity loop: seeded conversion, collection, score pose and release."""
from pathlib import Path
import re,json,hashlib,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/foe-bonus-integration';o.mkdir(exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();rh=sha(r/'out/rom.bin')
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
art=json.loads((r/'out/parity-audit/bonus-art-map.json').read_text());tiles=[]
for k,root in enumerate(art['roots']):
 fruit=art['frames'][root];score=art['frames'][fruit['next']];tiles.append([fruit['tile'],score['tile'],score['owner2_tile']])
s='''local cpu=manager.machine.devices[':maincpu'];local m=cpu.spaces.program
local frame=0;local scenario=0;local phase=-1;local checks=0;local terminal=false
local tiles=TILES
local function clear(a,n)for k=0,n-1 do m:write_u8(a+k,0)end end
local function check(v,why)checks=checks+1;assert(v,why..' scenario '..scenario..' phase '..phase)end
local function finish(err)terminal=true;local f=assert(io.open(RESULT,'w'));f:write(string.format('{"scenarios":%d,"checks":%d,"error":%s}',scenario,checks,err and string.format('%q',err)or'null'));f:close();manager.machine:exit()end
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  local family=scenario//22;local owner=(scenario//11)%2;local kind=scenario%11
  local a=FOES+family*120;local e=ENT+family*252;local p=GG+owner*42
  if phase>=0 then check(cpu.state['CURPC'].value&0xffffff==0xffb056,'return')end
  if phase==0 then
   check(m:read_u8(a+5)==65 and m:read_u8(a+7)==1,'conversion');check(m:read_u8(a+14)==kind,'kind')
   check(m:read_u16(e+40)==tiles[kind+1][1],'fruit tile');check((m:read_u16(0xffb404)&0x7ff)==tiles[kind+1][1],'fruit SAT')
   check(m:read_u8(NEXT)==(kind+1)%11,'index advance')
   m:write_u16(p,100);m:write_u16(p+2,103)
  elseif phase==1 then
   check(m:read_u8(a+5)==2 and m:read_u8(a+7)==2,'collection');check(m:read_u8(a+18)==owner,'owner')
   check(m:read_u32(p+28)==(kind==10 and 20000 or (kind+1)*1000),'score')
   check(m:read_u16(DIFF)==192,'difficulty');check(m:read_u8(p+38)==0,'pickup harmless')
  elseif phase==2 then
   check(m:read_u8(a+7)==3,'score phase');check(m:read_u16(e+40)==tiles[kind+1][owner+2],'score tile');check((m:read_u16(0xffb404)&0x7ff)==tiles[kind+1][owner+2],'score SAT')
   m:write_u16(a,248);m:write_u16(p,0)
  elseif phase==3 then check(m:read_u8(a+6)==6 and m:read_u8(a+7)==2,'offscreen')
  elseif phase==4 then check(m:read_u8(a+5)==255 and m:read_u8(a+7)==3,'release')
  elseif phase==5 then
   check(m:read_u8(a+5)==0 and m:read_u8(e)==0,'cleared');scenario=scenario+1;phase=-1
  end
  if scenario==44 then finish();return end
  family=scenario//22;owner=(scenario//11)%2;kind=scenario%11
  a=FOES+family*120;e=ENT+family*252;p=GG+owner*42
  if phase==-1 then
   clear(ENT,48*42);clear(GG,94);clear(FOES,240);clear(WEAPONS,72);clear(IA,96);clear(IW,10);clear(WAVES,106);clear(LARGE,102);clear(KPOOL,64)
   m:write_u8(SURVIVAL,0);m:write_u8(GG+84,2);m:write_u8(CLOCK,owner);m:write_u8(GG+(1-owner)*42+34,3)
   m:write_u8(NEXT,kind);m:write_u8(LIMIT,10);m:write_u16(DIFF,0)
   m:write_u16(a,100);m:write_u8(a+3,136);m:write_u8(a+5,1);m:write_u8(a+6,255)
   m:write_u8(e,1);m:write_u8(e+1,family==0 and 7 or 1)
  end
  phase=phase+1;m:write_u8(CLOCK,owner)
  for x=0xffb000,0xffb03e,2 do m:write_u16(x,0x4e71)end
  m:write_u16(0xffb040,0x4eb9);m:write_u32(0xffb042,UPDATE);m:write_u16(0xffb046,0x42a7);m:write_u16(0xffb048,0x4879);m:write_u32(0xffb04a,0xffb400);m:write_u16(0xffb04e,0x4eb9);m:write_u32(0xffb050,RENDER);m:write_u16(0xffb054,0x508f);m:write_u16(0xffb056,0x60fe)
  cpu.state['SR'].value=0x2700;cpu.state['SP'].value=0xffbfe0;cpu.state['PC'].value=0xffb000
 end)
 if not ok then finish(err)end
end)
'''
for key,name in {'ENT':'g_entities','GG':'g_game','FOES':'arcade_foes','WEAPONS':'arcade_weapons','IA':'arcade_i_actors','IW':'arcade_i_wave','WAVES':'arcade_waves','LARGE':'arcade_large_actors','KPOOL':'arcade_k_actors','NEXT':'arcade_bonus_next','LIMIT':'arcade_bonus_limit','DIFF':'g_arcadeCombatDifficulty','SURVIVAL':'g_survivalRun','CLOCK':'wave_clock','UPDATE':'entity_update_all','RENDER':'entity_render_all'}.items():s=re.sub(r'\b'+key+r'\b',str(syms[name]),s)
s=s.replace('TILES','{'+','.join('{'+','.join(map(str,row))+'}'for row in tiles)+'}').replace('RESULT',json.dumps(str(o/'results.json')))
(o/'run.lua').write_text(s);(o/'results.json').unlink(missing_ok=True)
with(o/'mame.log').open('w')as f:subprocess.run(['mame','genesis','-cart',str(r/'out/rom.bin'),'-window','-nomaximize','-resolution','512x448','-video','none','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_delay','0','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],stdout=f,stderr=subprocess.STDOUT,check=True,timeout=120)
assert sha(r/'out/rom.bin')==rh
d=json.loads((o/'results.json').read_text());d.update(rom_sha256=rh,scope='Injected live FF G/H conversion, pickup, score pose, release; natural powerup trigger and shared global lifecycle remain pending.')
(o/'results.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2));assert d['error']is None and d['scenarios']==44
