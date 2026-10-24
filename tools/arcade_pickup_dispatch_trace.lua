-- Original E2B3 alternating food/special contact call sites, empty actor pools.
local cpu=manager.machine.devices[':maincpu'];local m=cpu.spaces.program
local f=assert(io.open('out/parity-audit/pickup-dispatch-source.jsonl','w'))
local frame=0;local pending;local calls='';local done=false
pickup_dispatch_taps={}
for _,v in ipairs({{0xe2e0,'F0'},{0xe334,'F1'},{0xe2fb,'S0'},{0xe34f,'S1'}})do
 local address,label=v[1],v[2]
 pickup_dispatch_taps[#pickup_dispatch_taps+1]=m:install_read_tap(address,address,'pickup-'..label,function()if pending and calls:sub(-2)~=label then calls=calls..label end end)
end
local cases=coroutine.create(function()
 for clock=0,7 do for gates=0,3 do
  for a=0,0xfff do m:write_u8(a,0)end
  m:write_u8(0x24,clock);m:write_u8(0x88,gates&1);m:write_u8(0x89,(gates>>1)&1)
  for k,v in ipairs({0xbd,0xe2,0xb3,0x20,0xfe})do m:write_u8(0x1ff+k,v)end
  calls='';pending={clock,gates}
  cpu.state['S'].value=0x3f0;cpu.state['DP'].value=0;cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200
  coroutine.yield()
 end end
end)
emu.register_frame_done(function()
 if done then return end;frame=frame+1;if frame<60 then return end
 if frame==60 then m:write_u8(0x200,0x20);m:write_u8(0x201,0xfe);cpu.state['CC'].value=0x50;cpu.state['PC'].value=0x200;return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value==0x203,'return')
   local owner=pending[1]&1;local expected=((pending[2]>>owner)&1)==0 and ('F'..owner..'S'..owner)or''
   assert(calls==expected,calls..' expected '..expected)
   f:write(string.format('{"clock":%d,"gates":%d,"calls":"%s"}\n',pending[1],pending[2],calls));pending=nil
  end
  if coroutine.status(cases)=='dead'then done=true;f:close();print('PICKUP_DISPATCH_PASS 32');manager.machine:exit();return end
  local good,e=coroutine.resume(cases);assert(good,e)
 end)
 if not ok then done=true;f:close();print('PICKUP_DISPATCH_ERROR '..tostring(err));manager.machine:exit()end
end)
