-- Original G/H contact, shared E838 award and actual BCD score consumer.
local cpu=manager.machine.devices[':maincpu'];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/foe-bonus-collect.csv','w'))
f:write('family,index,owner,combat,active,phase,credit,event,sound,after_combat,points\n')
local frame=0;local pending=nil;local terminal=false
local cases=coroutine.create(function()
 for family=0,1 do for index=0,10 do for owner=0,1 do for _,combat in ipairs({0,0x1cff,0x3e80,0xffff})do
  for a=0,0x1ff do m:write_u8(a,0)end
  for a=0xc18,0xd97 do m:write_u8(a,0)end
  for a=0x480,0x4df do m:write_u8(a,0)end
  local base=family==0 and 0xc28 or 0xce8;local p=0x480+owner*48
  m:write_u8(base-16,65);m:write_u8(base-15,255);m:write_u8(base-14,1);m:write_u8(base-13,index);m:write_u8(base-12,1-owner)
  m:write_u16(base-11,100);m:write_u8(base-8,136)
  m:write_u8(p,1);m:write_u8(p+4,owner);m:write_u16(p+5,100);m:write_u8(p+8,136)
  m:write_u8(0xd5,owner*128);m:write_u16(0x96,combat);m:write_u8(0x22,1);m:write_u16(0x10,0x180);m:write_u16(0x14,0x1c0)
  local fn=family==0 and(owner==0 and 0xeac0 or 0xeacb)or(owner==0 and 0xeaa3 or 0xeaae)
  for a=0x200,0x21f do m:write_u8(a,0x12)end
  local code={0xbd,fn>>8,fn&255,0xf6,1,0x81,0xbd,0x4c,0xbe,0x20,0xfe}
  for k,v in ipairs(code)do m:write_u8(0x21f+k,v)end
  pending={family,index,owner,combat,base}
  cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;coroutine.yield()
 end end end end
end)
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value==0x229,'return')
   local p=pending;local base=p[5];p[5]=nil;local score=0
   for a=0x6c+p[3]*4,0x6f+p[3]*4 do local v=m:read_u8(a);assert((v&15)<10 and (v>>4)<10);score=score*100+(v>>4)*10+(v&15)end
   for _,v in ipairs({m:read_u8(base-16),m:read_u8(base-14),m:read_u8(base-12),m:read_u16(0x180),m:read_u8(0x1c1),m:read_u16(0x96),score})do p[#p+1]=v end
   f:write(table.concat(p,',')..'\n');pending=nil
  end
  if coroutine.status(cases)=='dead'then terminal=true;f:close();manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then terminal=true;f:close();print('BONUS_COLLECT_ERROR',err);manager.machine:exit()end
end)
