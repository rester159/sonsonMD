-- Execute original D74A with poisoned parent/child records to expose partial writes.
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/large-fire-oracle.csv','w'))
f:write('case,x,y,timer,period,p0x,p0y,p0active,p1x,p1y,p1active,parent_before,pool_before,parent_after,pool_after\n')
local frame=0;local pending=nil;local terminal=false
local function hex(a,n)local t={}for k=0,n-1 do t[#t+1]=string.format('%02x',m:read_u8(a+k))end;return table.concat(t)end
local cases=coroutine.create(function()
 for i=0,8191 do
  local x=({0,100,65530,65535})[(i//16)%4+1];local y=(i*37)&255
  local timer=({0,0,1,255})[(i//64)%4+1];local period=(i*17)&255
  local p0x=(x+({-1,0,1})[(i//256)%3+1])&65535;local p1x=(x+({1,0,-1})[(i//128)%3+1])&65535
  local p0y=(y+({7,8,12,16,17})[(i//7)%5+1])&255;local p1y=(y+({7,8,12,16,17})[(i//11)%5+1])&255
  local p0active=({0,1,2,255})[(i//3)%4+1];local p1active=({0,1,2,255})[(i//5)%4+1]
  for a=0xe38,0xe67 do m:write_u8(a,(a*7+i)&255)end
  for a=0xec8,0xf47 do m:write_u8(a,(a*13+i)&255)end
  for k=0,3 do m:write_u8(0xec8+k*32,(i&(1<<k))~=0 and 2 or 0)end
  m:write_u16(0xe3d,x);m:write_u8(0xe40,y);m:write_u8(0xe58,timer);m:write_u8(0xe59,period)
  m:write_u16(0x485,p0x);m:write_u8(0x488,p0y);m:write_u8(0x480,p0active)
  m:write_u16(0x4b5,p1x);m:write_u8(0x4b8,p1y);m:write_u8(0x4b0,p1active)
  pending={i,x,y,timer,period,p0x,p0y,p0active,p1x,p1y,p1active,hex(0xe38,48),hex(0xec8,128)}
  for a=0x200,0x21f do m:write_u8(a,0x12)end
  local code={0x8e,0x0e,0x48,0xbd,0xd7,0x4a,0x20,0xfe};for n,v in ipairs(code)do m:write_u8(0x21f+n,v)end
  cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;coroutine.yield()
 end
end)
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then assert(cpu.state['CURPC'].value==0x226,'fire return');pending[#pending+1]=hex(0xe38,48);pending[#pending+1]=hex(0xec8,128);f:write(table.concat(pending,',')..'\n');pending=nil end
  if coroutine.status(cases)=='dead'then terminal=true;f:close();print('LARGE_FIRE_DONE');manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then terminal=true;f:close();print('LARGE_FIRE_ERROR',err);manager.machine:exit()end
end)
