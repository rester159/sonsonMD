-- Original E55F/E6FF player/barrier scan with a RAM entry runway.
local cpu=manager.machine.devices[':maincpu'];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/barrier-player.csv','w'))
f:write('camera,dx,dy,active,pa,double,offset,x,state,phase,kind\n')
local frame=0;local pending;local done=false;local count=0
local cases=coroutine.create(function()
 for _,camera in ipairs({0,65530})do for _,dx in ipairs({-13,-12,-1,0,1,12,13})do
 for _,dy in ipairs({-6,-5,0,5,6})do for active=0,3 do for pa=0,2 do for double=0,1 do
 for _,offset in ipairs({6,100,256})do
  for a=0,0xfff do m:write_u8(a,0)end
  local ox=(camera+offset)&65535;local px=(ox+dx)&65535
  m:write_u8(0x480,pa);m:write_u8(0x481,2);m:write_u8(0x482,1);m:write_u8(0x483,0)
  m:write_u16(0x485,px);m:write_u8(0x488,(250+dy)&255);m:write_u16(0x90,camera)
  for _,b in ipairs({0x560,0x61e})do
   m:write_u8(b,(b==0x560 or double==1)and active or 0);m:write_u8(b+9,2)
   m:write_u16(b+4,ox);m:write_u8(b+6,250)
  end
  pending={camera,dx,dy,active,pa,double,offset}
  for a=0x200,0x21f do m:write_u8(a,0x12)end
  for k,v in ipairs({0xbd,0xe5,0x5f,0x20,0xfe})do m:write_u8(0x21f+k,v)end
  cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;coroutine.yield()
 end end end end end end end
end)
emu.register_frame_done(function()
 if done then return end;frame=frame+1;if frame<60 then return end
 if frame==60 then m:write_u8(0x200,0x20);m:write_u8(0x201,0xfe);cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value==0x223,string.format('return pc=%04x x=%04x y=%04x count=%d',cpu.state['CURPC'].value,cpu.state['X'].value,cpu.state['Y'].value,m:read_u8(0xcb)))
   for _,v in ipairs({m:read_u16(0x485),m:read_u8(0x481),m:read_u8(0x482),m:read_u8(0x483)})do pending[#pending+1]=v end
   f:write(table.concat(pending,',')..'\n');pending=nil;count=count+1
  end
  if coroutine.status(cases)=='dead'then done=true;f:close();print('BARRIER_PLAYER_PASS '..count);manager.machine:exit();return end
  local good,e=coroutine.resume(cases);assert(good,e)
 end)
 if not ok then done=true;f:close();print('BARRIER_PLAYER_ERROR '..tostring(err));manager.machine:exit()end
end)
