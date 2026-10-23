-- Execute original $E407 and $A73F..$A786. No program ROM modifications.
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local f=assert(io.open("out/parity-audit/difficulty-oracle.csv","w"))
f:write("operation,base,combat,amount,result_base,result_combat\n")
local frame=0;local pending=nil;local terminal=false
local cases=coroutine.create(function()
 local function run(op,base,combat,amount)
  m:write_u8(0x5a,base);m:write_u16(0x96,combat);m:write_u8(0x67,0)
  for a=0x200,0x21f do m:write_u8(a,0x12)end
  -- Load D inside wrapper; changing CPU registers mid-instruction is unreliable.
  m:write_u8(0x220,0xcc);m:write_u16(0x221,amount)
  m:write_u8(0x223,0xbd);m:write_u16(0x224,op==0 and 0xe407 or 0xa73f)
  m:write_u8(0x226,0x20);m:write_u8(0x227,0xfe)
  cpu.state["S"].value=0x3f0;cpu.state["DP"].value=0;cpu.state["CC"].value=0x50;cpu.state["PC"].value=0x200
  pending={op,base,combat,amount};coroutine.yield()
 end
 for _,amount in ipairs({0,0x19,0x33,0xffff})do
  for _,high in ipairs({0,15,27,28,47,62,63,255})do
   for low=0,255 do run(0,0,high*256+low,amount)end
  end
 end
 for base=0,255 do for _,combat in ipairs({0,0x19ff,0x3eff,0xffff})do run(1,base,combat,0)end end
end)
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end
 frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state["CURPC"].value==0x226,"original routine did not return")
   f:write(string.format("%d,%d,%d,%d,%d,%d\n",pending[1],pending[2],pending[3],pending[4],m:read_u8(0x5a),m:read_u16(0x96)));pending=nil
  end
  if coroutine.status(cases)=="dead"then terminal=true;f:close();print("DIFFICULTY_DONE");manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then terminal=true;f:close();print("DIFFICULTY_ERROR",err);manager.machine:exit()end
end)
