-- Execute original A697 including its actual column-expansion callees.
local cpu=manager.machine.devices[':maincpu'];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/scroll-signal.csv','w'))
f:write('camera,sub,latch,after_camera,after_sub,after_latch,advance\n')
local frame=0;local pending;local done=false;local count=0
local cases=coroutine.create(function()
 for _,camera in ipairs({0,7,8,15,255,256,65535})do for sub=0,255 do for _,latch in ipairs({0,8})do
  for a=0,0x1ff do m:write_u8(a,0)end
  m:write_u16(0x90,camera);m:write_u8(0x92,sub);m:write_u8(0x93,latch)
  m:write_u16(0x94,m:read_u16(0x6488));m:write_u16(0x8e,m:read_u16(0x648a));m:write_u8(0x98,5)
  for k,v in ipairs({0xbd,0xa6,0x97,0x20,0xfe})do m:write_u8(0x1ff+k,v)end
  pending={camera,sub,latch};cpu.state['S'].value=0x330;cpu.state['DP'].value=0
  cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;coroutine.yield()
 end end end
end)
emu.register_frame_done(function()
 if done then manager.machine:exit();return end
 frame=frame+1;if frame<60 then return end
 -- Establish a stable RAM loop before the first fixture (boot may be in SYNC).
 if frame==60 then
  m:write_u8(0x200,0x20);m:write_u8(0x201,0xfe)
  cpu.state['S'].value=0x330;cpu.state['DP'].value=0
  cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;return
 end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value==0x203,'return PC')
   local p=pending
   for _,v in ipairs({m:read_u16(0x90),m:read_u8(0x92),m:read_u8(0x93),m:read_u8(0x8b)})do p[#p+1]=v end
   f:write(table.concat(p,',')..'\n');pending=nil;count=count+1
  end
  if coroutine.status(cases)=='dead'then done=true;f:close();print('SCROLL_SIGNAL_TRACE '..count);manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then done=true;f:close();print('SCROLL_SIGNAL_ERROR '..tostring(err));manager.machine:exit()end
end)
