-- Full original hit leaves + original queued score handler. No ROM edits.
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local f=assert(io.open("out/parity-audit/i-reward-oracle.csv","w"))
f:write("family,owner,remaining,combat,after_combat,after_remaining,phase,style,event,bonus,score,enemy_active,shot_active,shot_state\n")
local frame=0;local pending=nil;local terminal=false
local cases=coroutine.create(function()
 for family=2,2 do for owner=0,1 do for remaining=0,255 do
 for _,combat in ipairs({0,0x1cff,0x3e80,0x3ed0,0x3ee7,0x3eff,0x3f00,0xffff})do
  local x=0xda8
  for a=0x180,0x1ff do m:write_u8(a,0)end
  for a=0x6c,0x77 do m:write_u8(a,0)end
  m:write_u8(0x22,1);m:write_u8(0x99,0);m:write_u8(0xe9,0)
  m:write_u8(x-16,1);m:write_u16(x-15,0x0201);m:write_u16(x-11,100);m:write_u8(x-8,128)
  m:write_u8(0x4d0,1);m:write_u16(0x4d1,0x0100)
  m:write_u8(0xc9,133);m:write_u16(0xca,108)
  m:write_u16(0x10,0x180);m:write_u16(0x14,0x1c0);m:write_u8(0xd5,owner*128)
  m:write_u8(0xb2+family,remaining);m:write_u16(0x96,combat)
  for a=0x200,0x21f do m:write_u8(a,0x12)end
  local code={0x8e,x>>8,x&255,0x10,0x8e,4,0xe0,0xbd, 0xea,0x3c,
   0xf6,1,0x81,0xbd,0x4c,0xbe}
  if remaining==1 then for _,v in ipairs({0xf6,1,0x83,0xbd,0x4c,0xbe})do code[#code+1]=v end end
  for i,v in ipairs(code)do m:write_u8(0x21f+i,v)end
  local stop=0x220+#code;m:write_u8(stop,0x20);m:write_u8(stop+1,0xfe)
  cpu.state["S"].value=0x3f0;cpu.state["DP"].value=0;cpu.state["CC"].value=0x50;cpu.state["PC"].value=0x200
  pending={family,owner,remaining,combat,x,stop};coroutine.yield()
 end end end end
end)
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then
   local p=pending;assert(cpu.state["CURPC"].value==p[6],"hit/score did not return")
   local score=0;for a=0x6c+p[2]*4,0x6f+p[2]*4 do local v=m:read_u8(a);assert((v&15)<10 and(v>>4)<10);score=score*100+(v>>4)*10+(v&15)end
   f:write(string.format("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",p[1],p[2],p[3],p[4],m:read_u16(0x96),m:read_u8(0xb2+p[1]),m:read_u8(p[5]-14),m:read_u8(p[5]-12),m:read_u16(0x180),m:read_u16(0x182),score,m:read_u8(p[5]-16),m:read_u8(0x4d0),m:read_u16(0x4d1)));pending=nil
  end
  if coroutine.status(cases)=="dead"then terminal=true;f:close();print("REWARD_DONE");manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then terminal=true;f:close();print("REWARD_ERROR",err);manager.machine:exit()end
end)
