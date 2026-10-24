-- Actual FED8 rendering, with deterministic mixed active states and unique codes.
local cpu=manager.machine.devices[':maincpu'];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/sprite-budget.jsonl','w'))
local frame=0;local pending;local done=false;local count=0
local bases={0x708,0x728,0x748,0x4e0,0x500,0x520,0x540,0x490,0x4c0}
for a=0x768,0xe28,0x20 do bases[#bases+1]=a end
for _,a in ipairs({0xe48,0xe78,0xea8,0xed8,0xef8,0xf18,0xf38})do bases[#bases+1]=a end
assert(#bases==71)
local function hex(a,n)local s=''for i=0,n-1 do s=s..string.format('%02x',m:read_u8(a+i))end return s end
local cases=coroutine.create(function()
 for scenario=0,255 do for solo=0,1 do
  for a=0,0x1ff do m:write_u8(a,0)end
  for a=0x3e0,0xf57 do m:write_u8(a,0)end
  m:write_u8(0x22,1);m:write_u8(0x89,solo);m:write_u16(0x90,0x1234)
  local flags='';local seed=scenario+1
  for i,b in ipairs(bases)do
   seed=(seed*25173+13849)&65535
   local options={0,0,0,0,1,2,65,128};local active=options[(seed>>8)%8+1]
   if scenario<72 then active=i<=scenario and 1 or 0 end
   flags=flags..string.format('%02x',active)
   m:write_u8(b-16,active);m:write_u16(b-11,0x1234+i*3);m:write_u8(b-8,(i*7)&255)
   if i>=65 and i<=67 then
    for k=0,3 do m:write_u8(b+k,i);m:write_u8(b+4+k,0xc0+k)end
   else m:write_u8(b,i);m:write_u8(b+1,0xc3)end
  end
  pending={scenario,solo,flags}
  for k,v in ipairs({0xbd,0xfe,0xd8,0x20,0xfe})do m:write_u8(0x1ff+k,v)end
  cpu.state['S'].value=0x330;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;coroutine.yield()
 end end
end)
emu.register_frame_done(function()
 if done then manager.machine:exit();return end
 frame=frame+1;if frame<60 then return end
 if frame==60 then m:write_u8(0x200,0x20);m:write_u8(0x201,0xfe);cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value==0x203,'return')
   f:write(string.format('{"scenario":%d,"solo":%d,"active":"%s","remaining":%d,"cells":"%s"}\n',pending[1],pending[2],pending[3],m:read_u8(0xa4),hex(0x3e0,96)))
   count=count+1;pending=nil
  end
  if coroutine.status(cases)=='dead'then done=true;f:close();print('SPRITE_BUDGET_TRACE '..count);manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then done=true;f:close();print('SPRITE_BUDGET_ERROR '..tostring(err));manager.machine:exit()end
end)
