-- Original G/H collision leaves; fixture supplies scan-prepared shot bounds.
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local f=assert(io.open("out/parity-audit/hitbox-oracle.csv","w"))
f:write("family,enemy_x,enemy_y,shot_x,shot_y,hit\n")
local frame=0;local pending=nil;local terminal=false
local cases=coroutine.create(function()
 local function run(family,ex,ey,sx,sy)
  ex=ex&65535;ey=ey&255;sx=sx&65535;sy=sy&255
  local x=family==0 and 0xc28 or 0xce8
  m:write_u8(x-16,1);m:write_u16(x-15,0x0201);m:write_u16(x-11,ex);m:write_u8(x-8,ey)
  m:write_u8(0x4d0,1);m:write_u16(0x4d1,0x0100)
  m:write_u8(0xc9,(sy+5)&255);m:write_u16(0xca,(sx+8)&65535)
  m:write_u16(0x10,0x180);m:write_u16(0x14,0x1c0);m:write_u8(0xd5,0)
  m:write_u8(0xb2+family,2);m:write_u16(0x96,0)
  for a=0x200,0x21f do m:write_u8(a,0x12)end
  m:write_u8(0x220,0x8e);m:write_u16(0x221,x)
  m:write_u8(0x223,0x10);m:write_u8(0x224,0x8e);m:write_u16(0x225,0x4e0)
  m:write_u8(0x227,0xbd);m:write_u16(0x228,family==0 and 0xe993 or 0xe8ea)
  m:write_u8(0x22a,0x20);m:write_u8(0x22b,0xfe)
  cpu.state["S"].value=0x3f0;cpu.state["DP"].value=0;cpu.state["CC"].value=0x50;cpu.state["PC"].value=0x200
  pending={family,ex,ey,sx,sy};coroutine.yield()
 end
 for family=0,1 do
  for _,center in ipairs({0,0xfff8})do
   for dx=-20,20 do for dy=-20,20 do run(family,center+dx,center+dy,center,center)end end
  end
  for d=0,255 do run(family,0x1234,d,0x1234,128);run(family,d*256,128,0,128)end
 end
end)
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state["CURPC"].value==0x22a,"collision did not return")
   f:write(string.format("%d,%d,%d,%d,%d,%d\n",pending[1],pending[2],pending[3],pending[4],pending[5],m:read_u8(0x4d0)==2 and 1 or 0));pending=nil
  end
  if coroutine.status(cases)=="dead"then terminal=true;f:close();print("HITBOX_DONE");manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then terminal=true;f:close();print("HITBOX_ERROR",err);manager.machine:exit()end
end)
