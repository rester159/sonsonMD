-- Original D55B/D7B4: complete kill and offscreen-exit lifetimes.
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/large-death-oracle.csv','w'));f:write('case,tick,subtype,variant,owner,exit,before,after\n')
local frame=0;local pending=nil;local terminal=false
local function hex(a,n)local t={}for k=0,n-1 do t[#t+1]=string.format('%02x',m:read_u8(a+k))end;return table.concat(t)end
local cases=coroutine.create(function()
 local id=0
 for subtype=0,2 do for variant=0,3 do for owner=0,1 do for exit=0,1 do
  for a=0xe38,0xe67 do m:write_u8(a,(a*7+id)&255)end
  m:write_u8(0xe38,2);m:write_u8(0xe39,2);m:write_u8(0xe3a,exit*4);m:write_u8(0xe3b,subtype);m:write_u8(0xe3c,owner);m:write_u8(0xe53,variant)
  m:write_u8(0x9d,0);m:write_u16(0xe46,0xd5ce)
  for tick=0,63 do
   pending={id,tick,subtype,variant,owner,exit,hex(0xe38,48)}
   for a=0x200,0x21f do m:write_u8(a,0x12)end
   local code={0x8e,0x0e,0x48,0xbd,0xd5,0x5b,0x20,0xfe};for k,v in ipairs(code)do m:write_u8(0x21f+k,v)end
   cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;coroutine.yield()
  end
  id=id+1
 end end end end
end)
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then assert(cpu.state['CURPC'].value==0x226,'death return');pending[#pending+1]=hex(0xe38,48);f:write(table.concat(pending,',')..'\n');pending=nil end
  if coroutine.status(cases)=='dead'then terminal=true;f:close();print('LARGE_DEATH_DONE');manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then terminal=true;f:close();print('LARGE_DEATH_ERROR',err);manager.machine:exit()end
end)
