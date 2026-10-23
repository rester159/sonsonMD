-- Original 6809 E7D6/E7DF special-pickup scan, FF conversion trigger.
-- Injected boundary fixtures; does not establish natural spawn reachability.
local cpu=manager.machine.devices[':maincpu'];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/conversion-trigger.csv','w'))
f:write('owner,limit,active,dx,dy,collected,after_limit,purge,next_index\n')
local frame=0;local pending;local done=false;local count=0
local cases=coroutine.create(function()
 for owner=0,1 do for _,limit in ipairs({0,1,2,3,4,255})do
 for _,active in ipairs({0,1,2})do for _,dx in ipairs({-4,-3,0,3,4})do
 for _,dy in ipairs({-8,-7,0,7,8})do
  for a=0,0x1ff do m:write_u8(a,0)end
  for a=0x480,0x4df do m:write_u8(a,0)end
  for a=0x6f8,0x757 do m:write_u8(a,0)end
  local p=0x480+owner*48;local b=0x718
  m:write_u8(p,1);m:write_u8(p+4,owner)
  m:write_u16(p+5,(2+dx)&65535);m:write_u8(p+8,(3+dy)&255)
  m:write_u8(b,active);m:write_u8(b+1,2);m:write_u8(b+2,7)
  m:write_u8(b+3,255);m:write_u8(b+4,1-owner)
  m:write_u16(b+5,2);m:write_u8(b+8,3)
  m:write_u8(0x7b,limit);m:write_u8(0xa2,9)
  local fn=owner==0 and 0xe7d6 or 0xe7df
  for k,v in ipairs({0xbd,fn>>8,fn&255,0x20,0xfe})do m:write_u8(0x1ff+k,v)end
  pending={owner,limit,active,dx,dy}
  cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0
  cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200
  coroutine.yield()
 end end end end end
end)
emu.register_frame_done(function()
 if done then manager.machine:exit();return end
 frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value==0x203,'return PC')
   local p=pending;local hit=p[3]==1 and math.abs(p[4])<=3 and math.abs(p[5])<=7
   local lim=(p[2]+1)&255;if lim>3 then lim=0 end
   assert(m:read_u8(0x718)==(hit and 2 or p[3]),'active')
   assert(m:read_u8(0x719)==(hit and 3 or 2),'state')
   assert(m:read_u8(0x71a)==(hit and 0 or 7),'phase')
   assert(m:read_u8(0x71c)==(hit and p[1] or 1-p[1]),'owner')
   assert(m:read_u8(0xa1)==(hit and 1 or 0),'purge')
   assert(m:read_u8(0xa2)==(hit and 0 or 9),'next index')
   assert(m:read_u8(0x7b)==(hit and lim or p[2]),'limit')
   for _,v in ipairs({hit and 1 or 0,m:read_u8(0x7b),m:read_u8(0xa1),m:read_u8(0xa2)})do p[#p+1]=v end
   f:write(table.concat(p,',')..'\n');pending=nil;count=count+1
  end
  if coroutine.status(cases)=='dead'then
   done=true;f:close();print('CONVERSION_TRIGGER_PASS '..count..' cases');manager.machine:exit();return
  end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then done=true;f:close();print('CONVERSION_TRIGGER_ERROR '..tostring(err));manager.machine:exit()end
end)
