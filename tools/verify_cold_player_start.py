#!/usr/bin/env python3
"""Observe real menu-to-game initialization without injecting player state."""
from pathlib import Path
import argparse,hashlib,json,re,subprocess
p=argparse.ArgumentParser();p.add_argument('--players',type=int,choices=(1,2),default=2);p.add_argument('--baseline',type=Path);p.add_argument('--expect-failure',action='store_true');args=p.parse_args()
r=Path(__file__).resolve().parents[1];b=args.baseline or r/'out';o=r/'out/parity-audit'/('cold-start-baseline'if args.baseline else f'cold-start-{args.players}p');o.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
rh=sha(b/'rom.bin');sh=sha(b/'symbol.txt')
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(b/'symbol.txt').read_text(),re.M)}
s='''local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local frame=0;local terminal=false;local tap=nil;local completed=nil;local observed=false;local bonus_words=0
local function finish(err)
 if terminal then return end;terminal=true
 local f=assert(io.open(RESULT,'w'));f:write(string.format('{"error":%s,"frame":%d,"bonus_art_words":%d}',err and string.format('%q',err)or'null',frame,bonus_words));f:close();manager.machine:exit()
end
local function pad(n,v)manager.machine.ioport.ports[":ctrl1:mdpad:PAD"].fields['P1 '..n]:set_value(v)end
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end
 frame=frame+1
 for _,n in ipairs({'Start','Down'})do pad(n,0)end
 if frame>=920 and frame<932 or PLAYERS==2 and frame>=1040 and frame<1052 then pad('Down',1)end
 if frame>=950 and frame<965 or frame>=1100 and frame<1112 then pad('Start',1)end
 if frame==60 then
  completed=m:install_write_tap(PRESENT+2,PRESENT+3,'first-start-update',function()
   if terminal or not observed then return end
   local ok,err=pcall(function()
    for k=0,PLAYERS-1 do
     local p=GG+k*42;local q=MOTION+k*14;local a=ANIM+k*5
     assert(m:read_u16(p)==32 and m:read_u16(p+4)==141,'first idle drift')
     assert(m:read_u16(p+2)==135-k*32,'first idle Y')
     assert(m:read_u8(q+5)==1 and m:read_u8(q+6)==0,'first normal state')
     assert(m:read_u8(a+1)==1 and m:read_u8(a+2)==3,'first animation cadence')
     assert(m:read_u8(a+3)==64+k*8 and m:read_u8(a+4)==192+k,'first animation pose')
    end
   end);finish(ok and nil or err)
  end)
  tap=m:install_read_tap(UPDATE,UPDATE+1,'cold-start',function()
   if terminal or observed or m:read_u8(ATTRACT)~=0 or m:read_u8(GG+84)~=PLAYERS then return end
   local ok,err=pcall(function()
    assert(m:read_u8(SURVIVAL)==0,'unexpected Survival')
    if BONUS_DATA~=0 then
     m:write_u16(0xc00004,0x8f02)
     local address=136*32;m:write_u32(0xc00004,((address&0x3fff)<<16)|((address&0xc000)>>14))
     for j=0,BONUS_WORDS-1 do
      assert(m:read_u16(0xc00000)==m:read_u16(BONUS_DATA+j*2),'bonus VRAM word '..j)
      bonus_words=bonus_words+1
     end
    end
    for k=0,PLAYERS-1 do
     local p=GG+k*42;local q=MOTION+k*14;local a=ANIM+k*5
     assert(m:read_u16(p)==32,'startup X owner '..k)
     assert(m:read_u16(p+2)==135-k*32,'startup Y owner '..k..' got '..m:read_u16(p+2))
     assert(m:read_u16(p+4)==0 and m:read_u16(p+6)==0,'cold fractions')
     assert(m:read_u8(q+5)==0 and m:read_u8(q+6)==0,'startup state/phase')
     assert(m:read_u8(a)==k and m:read_u8(a+1)==0 and m:read_u8(a+2)==0,'startup animation')
     assert(m:read_u8(AVAILABLE+k)==2,'shot availability')
    end
   end);if not ok then finish(err)else observed=true end
  end)
 end
 if frame>2400 then finish('menu/start timeout')end
end)
'''
for key,name in {'PRESENT':'g_presentCounter','UPDATE':'game_update','ATTRACT':'g_attract','SURVIVAL':'g_survivalRun','GG':'g_game','MOTION':'player_motion','ANIM':'player_animation','AVAILABLE':'player_shot_available'}.items():s=re.sub(r'\b'+key+r'\b',str(syms[name]),s)
s=s.replace('PLAYERS',str(args.players))
s=s.replace('BONUS_DATA',str(syms.get('arcade_bonus_pixels',0))).replace('BONUS_WORDS',str(int(re.search(r'#define ARCADE_BONUS_TILE_COUNT (\d+)',(r/'inc/arcade_bonus_art.h').read_text()).group(1))*16))
s=s.replace('RESULT',json.dumps(str(o/'results.json')));(o/'run.lua').write_text(s);(o/'results.json').unlink(missing_ok=True)
with (o/'mame.log').open('w')as f:subprocess.run(['mame','genesis','-cart',str(b/'rom.bin'),'-window','-nomaximize','-resolution','512x448','-video','none','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_delay','0','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],stdout=f,stderr=subprocess.STDOUT,check=True,timeout=120)
assert sha(b/'rom.bin')==rh and sha(b/'symbol.txt')==sh
result=json.loads((o/'results.json').read_text());result.update(rom_sha256=rh,symbols_sha256=sh,scope=f'Controller-driven Home {args.players}P cold start, initialization and first completed game_update; no player state injection')
(o/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(result)
assert (result['error']is not None)==args.expect_failure
