#!/usr/bin/env python3
"""Capture all source RAM and source SAT at completed simulation checkpoints.

MAME write taps observe publication of SS2_PERF_INPUT's completed-work index;
no target-CPU hash or memory-copy work is added. BUILD_DIR OUTPUT_DIR [TICKS]
"""
import json,re,subprocess,sys
from pathlib import Path
build=Path(sys.argv[1]).resolve();out=Path(sys.argv[2]).resolve();out.mkdir(parents=True,exist_ok=True)
end=int(sys.argv[3]) if len(sys.argv)>3 else 4096
sym={n:int(a,16)&0xffffff for a,n in re.findall(r'^([0-9a-f]+)\s+\w\s+(\S+)$',(build/'symbol.txt').read_text(),re.M)}
def addr(name):return next(v for n,v in sym.items() if n==name or n.startswith(name+'.'))
lua='''local mem=manager.machine.devices[':maincpu'].spaces.program
local fr=0
local last=0
local done=false
state_tap=mem:install_write_tap(INDEX,INDEX+1,'completed_state',function(offset,data,mask)
 local tick=mem:read_u16(TICK)
 if tick>0 and tick%256==0 and tick~=last and tick<=ENDTICK then
  last=tick
  local bytes={}
  for i=0,8191 do bytes[#bytes+1]=string.char(mem:read_u8(RAM+i))end
  for i=0,511 do bytes[#bytes+1]=string.char(mem:read_u8(SAT+i))end
  local f=assert(io.open(OUT..string.format('/state-%04d.bin',tick),'wb'))
  f:write(table.concat(bytes));f:close()
  if tick==ENDTICK then done=true end
 end
end)
emu.register_frame_done(function()
 fr=fr+1
 for _,port in pairs(manager.machine.ioport.ports)do
  for _,name in ipairs({'P1 Start','P1 Right'})do
   local field=port.fields[name]
   if field then
    local on=(name=='P1 Start' and ((fr>=60 and fr<68)or(fr>=180 and fr<188)))or(name=='P1 Right'and fr>=120 and fr<128)
    field:set_value(on and 1 or 0)
   end
  end
 end
 if done then manager.machine:exit()end
end)
'''
for k,v in {'INDEX':hex(addr('ss2_perf_index')),'TICK':hex(addr('ss2_loop_n')),'RAM':hex(addr('ss2_ram')),'SAT':hex(addr('ss2_sat')),'ENDTICK':str(end),'OUT':json.dumps(str(out))}.items():lua=re.sub(r'\b'+k+r'\b',v,lua)
script=out/'capture.lua';script.write_text(lua)
with (out/'run.log').open('w') as f:
 subprocess.run(['mame','genesis','-cart',str(build/'rom.bin'),'-video','none','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_delay','0','-autoboot_script',str(script),'-seconds_to_run','180'],stdout=f,stderr=subprocess.STDOUT,check=True,timeout=180)
for tick in range(256,end+1,256):assert (out/f'state-{tick:04d}.bin').stat().st_size==8704
print(f'Captured {end//256} completed RAM/SAT checkpoints')
