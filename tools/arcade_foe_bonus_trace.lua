-- Original G/H dispatchers, seeded shared FF conversion states.
local cpu=manager.machine.devices[':maincpu'];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/foe-bonus-oracle.jsonl','w'))
local frame=0;local pending=nil;local terminal=false
local function hex(a,n)local t={}for i=0,n-1 do t[#t+1]=string.format('%02x',m:read_u8(a+i))end;return table.concat(t)end
local cases=coroutine.create(function()
 for family=0,1 do for phase=0,3 do for _,active in ipairs({1,2,65,128})do for index=0,10 do for _,x in ipairs({0,8,247,248})do
  for a=0xc18,0xd97 do m:write_u8(a,0)end
  local base=family==0 and 0xc28 or 0xce8
  m:write_u8(base-16,active);m:write_u8(base-15,255);m:write_u8(base-14,phase)
  m:write_u8(base-13,index);m:write_u8(base-12,index%2)
  m:write_u16(base-11,0x2345+x);m:write_u8(base-8,136);m:write_u8(base-6,3)
  m:write_u16(base-2,m:read_u16(0xb291+index*2));m:write_u8(base-3,0)
  m:write_u8(0xa2,index);m:write_u8(0x7b,10);m:write_u8(0x8a,0);m:write_u8(0xa1,0);m:write_u16(0x90,0x2345)
  pending={family=family,phase=phase,active=active,index=index,x=x,base=base,before=hex(base-16,32)}
  for a=0x200,0x21f do m:write_u8(a,0x12)end
  m:write_u8(0x220,0xbd);m:write_u16(0x221,family==0 and 0xc69b or 0xcefa);m:write_u8(0x223,0x20);m:write_u8(0x224,0xfe)
  cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200
  coroutine.yield()
 end end end end end
end)
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value==0x223,'dispatcher return')
   local p=pending
   f:write(string.format('{"family":%d,"phase":%d,"active":%d,"index":%d,"x":%d,"next_index":%d,"before":"%s","after":"%s"}\n',p.family,p.phase,p.active,p.index,p.x,m:read_u8(0xa2),p.before,hex(p.base-16,32)))
   pending=nil
  end
  if coroutine.status(cases)=='dead'then terminal=true;f:close();manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then terminal=true;f:close();print('BONUS_TRACE_ERROR',err);manager.machine:exit()end
end)
