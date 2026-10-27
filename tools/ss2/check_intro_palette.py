#!/usr/bin/env python3
"""Reproduce skipped-intro colours on a linked Collection ROM in MAME.

Usage: check_intro_palette.py BUILD_DIR OUTPUT_DIR [watch]
Default skips the intro with B; watch lets it finish. Injects only an ordinary
platform object after gameplay begins, to keep the visual check reproducible.
No palette, sprite-cell or renderer state is injected. ROM symbols are required.
"""
from pathlib import Path
import re,subprocess,sys
root=Path(__file__).resolve().parents[2]
build=Path(sys.argv[1]).resolve();out=Path(sys.argv[2]).resolve();out.mkdir(parents=True,exist_ok=True)
watch=len(sys.argv)>3 and sys.argv[3]=='watch'
sym={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(build/'symbol.txt').read_text(),re.M)}
lua='''local fr=0
local mem=manager.machine.devices[':maincpu'].spaces.program
local ram=RAM
local injected=false
local age=0
emu.register_frame_done(function()
 fr=fr+1
 for _,port in pairs(manager.machine.ioport.ports)do
  for _,name in ipairs({'P1 Start','P1 Right','P1 B'})do
   local field=port.fields[name]
   if field then
    local on=false
    if name=='P1 Start'then on=(fr>=60 and fr<68) or (fr>=180 and fr<188) or (fr>300 and fr<900 and fr%120<8)end
    if name=='P1 Right'then on=fr>=120 and fr<128 end
    if name=='P1 B'then on=SKIP and fr>900 and fr<1200 and fr%30<5 end
    field:set_value(on and 1 or 0)
   end
  end
 end
 -- $6D is the source intro flag; scene zero is gameplay. Wait until fade-in.
 if fr>1400 and mem:read_u8(ram+0x6d)==0 and mem:read_u8(SCENE)==0 then age=age+1 end
 if age==30 then
  local dst={0x245A,0x267A,0x269A,0x26BA,0x26DA,0x26FA,0x271A,0x273A,0x275A,0x279A,0x27FA,0x289A,0x255A,0x253A,0x27DA,0x27BA,0x28DA,0x28BA,0x291A,0x28FA,0x295A,0x293A}
  local rec={0x80,1,0x2b,2,0,0,0x1c,8,5,0,0,0,0x8e,0,0xa0,0x2d,0x2f,0x8c,0x67,0xee,0x75,0x8c}
  local function w(a,v)mem:write_u8(ram+a-0x2000+31,v)end
  for j,a in ipairs(dst)do w(a,rec[j])end
  w(0x247a,0);w(0x249a,140);w(0x24da,0);w(0x24fa,140)
  w(0x2b1a,0x1c);w(0x2b3a,8);w(0x283a,0);w(0x2ada,0);w(0x2afa,0);w(0x287a,1)
  injected=true
 end
 if age==90 then
  manager.machine.video:snapshot()
  local pal=mem:read_u8(INTRO)
  print('INTRO_PALETTE '..pal)
  print('PLATFORM_INJECTED '..tostring(injected))
  manager.machine:exit()
 end
end)
'''
for k,v in {'RAM':hex(sym['ss2_ram']),'SCENE':hex(sym['scene_shown']),'INTRO':hex(sym['ss2_pal_intro']),'SKIP':'false' if watch else 'true'}.items():
 lua=re.sub(r'\b'+k+r'\b',v,lua)
script=out/'probe.lua';script.write_text(lua)
with (out/'run.log').open('w') as f:
 subprocess.run(['mame','genesis','-cart',str(build/'rom.bin'),'-video','none','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_delay','0','-autoboot_script',str(script),'-snapshot_directory',str(out/'screens'),'-seconds_to_run','100'],cwd=root,stdout=f,stderr=subprocess.STDOUT,timeout=180,check=True)
log=(out/'run.log').read_text();assert 'INTRO_PALETTE 0' in log and 'PLATFORM_INJECTED true' in log,log
print('PASS: gameplay uses stage sprite colours after '+('watching' if watch else 'skipping')+' intro')
