-- EE10 shot scan/cancel and ED9F ordinary contact through original CPU.
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/k-collision-oracle.csv','w'));f:write('case,operation,owner,actor_before,pool_before,actor_after,pool_after,selected,event\n')
local frame=0;local pending=nil;local terminal=false
local function hex(a,n)local t={}for k=0,n-1 do t[#t+1]=string.format('%02x',m:read_u8(a+k))end;return table.concat(t)end
local cases=coroutine.create(function()
 for i=0,8191 do
  local op=i&1;local owner=(i>>3)&1;local actor=op==0 and 0x4d0 or 0x480
  local x=({0,100,65535})[(i//4)%3+1];local y=(i*37)&255
  for k=0,47 do m:write_u8(actor+k,(i+k*13)&255)end
  m:write_u8(actor,({0,1,1,2})[(i//2)%4+1]);m:write_u16(actor+1,0x0207);m:write_u8(actor+3,0);m:write_u16(actor+5,x);m:write_u8(actor+8,y)
  for k=0,3 do
   local a=0xec8+k*32
   for j=0,31 do m:write_u8(a+j,(i+j*7+k)&255)end
   m:write_u8(a,({0,1,1,2,3,128,129})[(i//(k+2)+k)%7+1]);m:write_u8(a+1,({0,2,3,127,128,255})[(i//(k+3))%6+1]);m:write_u8(a+3,2)
   local dx=({-10,-9,-7,-6,0,6,7,9,10})[(i//7+k)%9+1];local dy=({-8,-7,-6,0,6,7,8})[(i//11+k)%7+1]
   m:write_u16(a+5,(x+dx)&65535);m:write_u8(a+8,(y+dy)&255)
  end
  m:write_u16(0x10,0x180);m:write_u16(0x180,0);m:write_u8(0xd5,owner*128)
  pending={i,op,owner,hex(actor,32),hex(0xec8,128),actor}
  for a=0x200,0x21f do m:write_u8(a,0x12)end
  local pc=op==0 and 0xee10 or 0xed9f
  local code={0x8e,(actor+16)>>8,(actor+16)&255,0xbd,pc>>8,pc&255,0x20,0xfe};for k,v in ipairs(code)do m:write_u8(0x21f+k,v)end
  cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;coroutine.yield()
 end
end)
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value==0x226,'collision return')
   local p=pending;local actor=p[6];p[6]=hex(actor,32);p[7]=hex(0xec8,128)
   local hit=p[2]==0 and m:read_u16(0x180)~=0 or p[2]==1 and m:read_u8(actor+3)==1
   p[8]=hit and (cpu.state['U'].value-0xed8)//32 or 255;p[9]=m:read_u16(0x180)
   f:write(table.concat(p,',')..'\n');pending=nil
  end
  if coroutine.status(cases)=='dead'then terminal=true;f:close();print('K_COLLISION_DONE');manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then terminal=true;f:close();print('K_COLLISION_ERROR',err);manager.machine:exit()end
end)
