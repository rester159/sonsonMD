-- Original A787 section-clear gate and timeout, isolated original CPU calls.
local cpu=manager.machine.devices[':maincpu'];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/stop-gate.csv','w'))
f:write('camera,clock,timer,grade,scenario,after_timer,after_grade,stop,release,bonus,variant,placement,kind\n')
local frame=0;local pending;local done=false;local count=0
local cases=coroutine.create(function()
 for _,camera in ipairs({0,4000,65530})do for _,clock in ipairs({0,1,63,64})do
 for _,timer in ipairs({0,1,2,255})do for grade=0,7 do for scenario=0,3 do
  for a=0,0xfff do m:write_u8(a,0)end
  m:write_u16(0x90,camera);m:write_u8(0x24,clock);m:write_u8(0x9d,1)
  m:write_u8(0x9f,timer);m:write_u8(0x7d,grade)
  -- Last pool slot tests the complete scan; edge is unsigned and inclusive.
  local b=0x560+19*10
  m:write_u8(b,scenario==0 and 0 or 4);m:write_u8(b+9,scenario==3 and 1 or 2)
  m:write_u16(b+4,(camera+256+(scenario==2 and 1 or 0))&65535)
  pending={camera,clock,timer,grade,scenario}
  for k,v in ipairs({0xbd,0xa7,0x87,0x20,0xfe})do m:write_u8(0x1ff+k,v)end
  cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;coroutine.yield()
 end end end end end
end)
emu.register_frame_done(function()
 if done then return end;frame=frame+1;if frame<60 then return end
 if frame==60 then m:write_u8(0x200,0x20);m:write_u8(0x201,0xfe);cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value==0x203,'return')
   for _,v in ipairs({m:read_u8(0x9f),m:read_u8(0x7d),m:read_u8(0x9d),m:read_u16(0xf48),m:read_u16(0x550),m:read_u8(0x70a),m:read_u8(0x554),m:read_u8(0x6fb)})do pending[#pending+1]=v end
   f:write(table.concat(pending,',')..'\n');pending=nil;count=count+1
  end
  if coroutine.status(cases)=='dead'then done=true;f:close();print('STOP_TRACE_PASS '..count);manager.machine:exit();return end
  local good,e=coroutine.resume(cases);assert(good,e)
 end)
 if not ok then done=true;f:close();print('STOP_TRACE_ERROR '..tostring(err));manager.machine:exit()end
end)
