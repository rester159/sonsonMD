-- Isolated original B0D1 descriptor pair. Full output includes background strip.
local cpu=manager.machine.devices[':maincpu'];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/special-spawn.jsonl','w'))
local frame=0;local pending;local done=false;local count=0
local function hex(a,n)local s='' for i=0,n-1 do s=s..string.format('%02x',m:read_u8(a+i))end return s end
local cases=coroutine.create(function()
 for slot=0,1 do for phase=0,5 do for gates=0,7 do for active=0,1 do
 for _,placement in ipairs({0,0x68,0xf8,0xff})do
  for a=0,0x1ff do m:write_u8(a,0)end
  for a=0x550,0x55f do m:write_u8(a,0)end
  for a=0x6f8,0x737 do m:write_u8(a,0)end
  for a=0x3a0,0x3e0 do m:write_u8(a,0x55)end
  local d=0x550+slot*8;local b=0x6f8+slot*32
  m:write_u8(d,active);m:write_u8(d+1,phase);m:write_u8(d+4,placement)
  m:write_u8(d+5,placement>>3);m:write_u16(d+6,0xb18a)
  m:write_u8(b,2);m:write_u8(b+1,3);m:write_u8(b+2,7)
  m:write_u8(b+3,255);m:write_u8(b+28,99)
  m:write_u8(0x9d,gates&1);m:write_u8(0xa4,(gates>>1)&1);m:write_u8(0x8b,(gates>>2)&1)
  pending={slot,phase,gates,active,placement,d,b}
  for k,v in ipairs({0xbd,0xb0,0xd1,0x20,0xfe})do m:write_u8(0x1ff+k,v)end
  cpu.state['S'].value=0x330;cpu.state['DP'].value=0
  cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;coroutine.yield()
 end end end end end
end)
emu.register_frame_done(function()
 if done then manager.machine:exit();return end
 frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value==0x203,'return PC')
   local p=pending
   f:write(string.format('{"slot":%d,"phase":%d,"gates":%d,"active":%d,"placement":%d,"descriptor":"%s","object":"%s","strip":"%s"}\n',p[1],p[2],p[3],p[4],p[5],hex(p[6],8),hex(p[7],32),hex(0x3a0,65)))
   pending=nil;count=count+1
  end
  if coroutine.status(cases)=='dead'then done=true;f:close();print('SPECIAL_SPAWN_TRACE '..count);manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then done=true;f:close();print('SPECIAL_SPAWN_ERROR '..tostring(err));manager.machine:exit()end
end)
