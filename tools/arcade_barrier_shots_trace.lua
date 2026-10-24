-- Original EB11 barrier-shot scan, isolated calls with a RAM entry runway.
local cpu=manager.machine.devices[':maincpu'];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/barrier-shots.csv','w'))
f:write('dx,dy,active,shot,double,nextphase,phase0,phase19,shot_active,shot_state,shot_phase,events\n')
local frame=0;local pending;local done=false;local count=0
local cases=coroutine.create(function()
 for _,dx in ipairs({-15,-14,-13,0,7,8,9})do for _,dy in ipairs({-11,-10,0,10,11})do
 for active=0,3 do for shot=0,2 do for double=0,1 do for nextphase=3,4 do
  for a=0,0xfff do m:write_u8(a,0)end
  m:write_u8(0x4d0,shot);m:write_u16(0x4d5,65530);m:write_u8(0x4d8,250)
  for _,b in ipairs({0x560,0x61e})do
   m:write_u8(b,(b==0x560 or double==1)and active or 0);m:write_u8(b+1,3)
   m:write_u16(b+2,0x300);m:write_u16(b+4,(65530+dx)&65535)
   m:write_u8(b+6,(250+dy)&255);m:write_u8(b+9,2)
  end
  m:write_u8(0x304,nextphase);m:write_u16(0x10,0x180)
  pending={dx,dy,active,shot,double,nextphase}
  for a=0x200,0x21f do m:write_u8(a,0x12)end
  for k,v in ipairs({0x10,0x8e,4,0xe0,0xbd,0xeb,0x11,0x20,0xfe})do m:write_u8(0x21f+k,v)end
  cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;coroutine.yield()
 end end end end end end
end)
emu.register_frame_done(function()
 if done then return end;frame=frame+1;if frame<60 then return end
 if frame==60 then m:write_u8(0x200,0x20);m:write_u8(0x201,0xfe);cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value==0x227,string.format('return pc=%04x x=%04x y=%04x count=%d',cpu.state['CURPC'].value,cpu.state['X'].value,cpu.state['Y'].value,m:read_u8(0xcb)))
   for _,v in ipairs({m:read_u8(0x561),m:read_u8(0x61f),m:read_u8(0x4d0),m:read_u8(0x4d1),m:read_u8(0x4d2),(m:read_u16(0x10)-0x180)/2})do pending[#pending+1]=v end
   f:write(table.concat(pending,',')..'\n');pending=nil;count=count+1
  end
  if coroutine.status(cases)=='dead'then done=true;f:close();print('BARRIER_SHOTS_PASS '..count);manager.machine:exit();return end
  local good,e=coroutine.resume(cases);assert(good,e)
 end)
 if not ok then done=true;f:close();print('BARRIER_SHOTS_ERROR '..tostring(err));manager.machine:exit()end
end)
