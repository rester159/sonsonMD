-- Original AD10 firing gate, stable slot choice and partial shot-record writes.
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local f=assert(io.open("out/parity-audit/fire-effects-oracle.jsonl","w"))
local frame=0;local pending=nil;local terminal=false
local function hex(a,n)local t={}for k=0,n-1 do t[#t+1]=string.format('%02x',m:read_u8(a+k))end;return table.concat(t)end
local cases=coroutine.create(function()
 local case=0
 for _,available in ipairs({0,2})do for input=0,1 do for _,previous in ipairs({0,1})do
 for kind=0,1 do for _,state in ipairs({1,5})do for _,facing in ipairs({1,3})do for _,active in ipairs({0})do
  local x=case%2==0 and 0xffff or 0x1234;local y=case%3==0 and 4 or 136
  for k=0,63 do m:write_u8(0x4d0+k,(k*7+case)%256)end
  m:write_u8(0x4d0,active);m:write_u8(0x4f0,2)
  m:write_u16(0x47e,0x4e0);m:write_u16(0x485,x);m:write_u8(0x488,y);m:write_u8(0x48a,facing);m:write_u8(0x481,state);m:write_u8(0x483,kind)
  m:write_u8(0x49c,available);m:write_u8(0x498,input);m:write_u8(0x499,previous);m:write_u8(0x22,1);m:write_u16(0x14,0x1c0)
  m:write_u8(0x1c0,0);m:write_u8(0x1c1,0)
  pending={case=case,x=x,y=y,facing=facing,state=state,kind=kind,available=available,input=input,previous=previous,active=active,before=hex(0x4d0,64),parent=hex(0x480,48),sound=-1}
  for a=0x200,0x21f do m:write_u8(a,0x12)end
  local code={0x8e,4,0x90,0xbd,0xad,0x10,0x20,0xfe};for i,v in ipairs(code)do m:write_u8(0x21f+i,v)end
  cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200
  coroutine.yield();case=case+1
 end end end end end end end
end)
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value==0x226,'fire did not return');local p=pending
   assert(hex(0x480,48)==p.parent,'AD10 changed player animation/state')
   local expected=(p.available~=0 and p.input==1 and p.previous==0)and 8 or -1
   p.sound=m:read_u16(0x14)==0x1c2 and m:read_u8(0x1c1)or -1
   assert(p.sound==expected,'source fire sound '..p.sound..' expected '..expected)
   f:write(string.format('{"case":%d,"sound":%d,"parent_unchanged":true}\n',p.case,p.sound));pending=nil
  end
  if coroutine.status(cases)=='dead'then terminal=true;f:close();print('FIRE_EFFECTS_DONE');manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then terminal=true;f:close();print('FIRE_EFFECTS_ERROR',err);manager.machine:exit()end
end)
