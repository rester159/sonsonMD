-- Original F97A controller. Actors are observed, never translated on target.
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local f=assert(io.open("out/parity-audit/i-wave-oracle.csv","w"))
f:write("active,state,phase,wait,parameter,cursor,remaining,live,clock,camera,reservation,after_state,after_phase,after_wait,after_cursor,after_remaining,after_reservation,birth,birth_x,birth_y,birth_facing\n")
local frame=0;local pending=nil;local terminal=false
local cases=coroutine.create(function()
 for active=0,1 do for _,live in ipairs({0,1,4})do for _,clock in ipairs({0,28,29,30,31,255})do
 for _,wait in ipairs({0,1,2,255})do for _,parameter in ipairs({0,1,129,255})do for mode=0,4 do
 for _,remaining in ipairs({0,1,4})do
  local state=mode>=3 and 1 or 0;local phase=mode>=3 and mode-3 or mode;local cursor=4-remaining
  local camera=clock==255 and 65520 or 9029
  for a=0xd98,0xe17 do m:write_u8(a,0)end
  m:write_u8(0x6a8,active);m:write_u8(0x6a9,wait);m:write_u8(0x6aa,state);m:write_u8(0x6ab,phase)
  m:write_u16(0x6ac,0xda8+cursor*32);m:write_u8(0x6ae,remaining);m:write_u8(0x6b6,parameter)
  m:write_u8(0xac,live);m:write_u8(0x24,clock);m:write_u16(0x90,camera);m:write_u8(0xb4,9)
  pending={active,state,phase,wait,parameter,cursor,remaining,live,clock,camera,9}
  for a=0x200,0x21f do m:write_u8(a,0x12)end
  m:write_u8(0x220,0xbd);m:write_u16(0x221,0xf97a);m:write_u8(0x223,0x20);m:write_u8(0x224,0xfe)
  cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200
  coroutine.yield()
 end end end end end end end
end)
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value==0x223,'controller did not return')
   local b=-1;for k=0,3 do if m:read_u8(0xd98+k*32)~=0 then assert(b==-1);b=k end end
   local p=pending;local a=0xda8+(b<0 and 0 or b)*32
   f:write(table.concat(p,',')..string.format(',%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n',m:read_u8(0x6aa),m:read_u8(0x6ab),m:read_u8(0x6a9),(m:read_u16(0x6ac)-0xda8)//32,m:read_u8(0x6ae),m:read_u8(0xb4),b,b<0 and 0 or m:read_u16(a-11),b<0 and 0 or m:read_u8(a-8),b<0 and 0 or m:read_u8(a-6)));pending=nil
  end
  if coroutine.status(cases)=='dead'then terminal=true;f:close();print('I_WAVE_DONE');manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then terminal=true;f:close();print('I_WAVE_ERROR',err);manager.machine:exit()end
end)
