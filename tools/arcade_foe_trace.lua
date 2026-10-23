-- Isolated original CPU G/H dispatch + straight-weapon dispatch. No ROM edits.
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local path=os.getenv("SONSON_FOE_TRACE")or"out/parity-audit/foe-oracle.jsonl"
local f=assert(io.open(path,"w"));local frame=0;local pending=nil;local terminal=false;local floor_tap=nil
local function hex(a,n)local t={}for i=0,n-1 do t[#t+1]=string.format("%02x",m:read_u8(a+i))end;return table.concat(t)end
local function clear(a,n)for i=0,n-1 do m:write_u8(a+i,0)end end
local function invoke(family)
 for a=0x200,0x21f do m:write_u8(a,0x12)end
 m:write_u8(0x220,0xbd);m:write_u16(0x221,family==0 and 0xc69b or 0xcefa)
 m:write_u8(0x223,0xbd);m:write_u16(0x224,0xcbc8)
 m:write_u8(0x226,0x20);m:write_u8(0x227,0xfe)
 cpu.state["S"].value=0x3f0;cpu.state["DP"].value=0;cpu.state["CC"].value=0x50;cpu.state["PC"].value=0x200
end
local cases=coroutine.create(function()
 for family=0,1 do for _,slot in ipairs({0,3,5})do for scenario=0,7 do
  clear(0xc18,0x180);clear(0xec8,128);clear(0x480,0x60)
  local base=(family==0 and 0xc28 or 0xce8)+slot*32
  local camera=0x2345;local frac=0
  m:write_u8(base-16,128);m:write_u16(base-11,camera+(scenario==7 and -8 or 248));m:write_u8(base-9,scenario%2==0 and 0 or 255);m:write_u8(base-8,136);m:write_u8(base-6,scenario==7 and 1 or 3);m:write_u8(base+13,scenario*3)
  m:write_u8(0xa1,0);m:write_u8(0xaa,0);m:write_u8(0xab,0);m:write_u8(0x8a,0)
  for col=0,31 do for row=0,3 do m:write_u8(0x100+col*4+row,({8,136,136,128})[row+1])end end
  if scenario==3 then for col=8,12 do for row=0,3 do m:write_u8(0x100+col*4+row,0)end end end
  for tick=0,399 do
   local y=136
   if scenario==1 then y=200 elseif scenario==2 then y=72 elseif scenario==3 then y=({200,72,136,168,104})[(tick//80)%5+1]end
   m:write_u8(0x480,scenario==5 and 0 or 1);m:write_u16(0x485,camera+80);m:write_u8(0x488,y);m:write_u8(0x483,scenario==6 and 1 or 0)
   m:write_u8(0x4b0,scenario==4 and 1 or 0);m:write_u16(0x4b5,camera+180);m:write_u8(0x4b8,200);m:write_u8(0x4b3,0)
   m:write_u8(0x99,scenario==4 and 1 or 0);m:write_u8(0x24,tick%256);m:write_u8(0x96,scenario==7 and 24 or 0);m:write_u8(0x9d,0);m:write_u8(0xa4,scenario==7 and 0 or 1);m:write_u16(0x90,camera)
   if tick==220 and scenario>=4 and m:read_u8(base-16)~=0 then m:write_u8(base-15,6);m:write_u8(base-14,scenario==7 and 4 or 0);m:write_u8(base-12,scenario%2)end
   pending={family=family,slot=slot,scenario=scenario,tick=tick,camera=camera,base=base,floor_x=-1,floor_y=-1,before=hex(base-16,32),weapons=hex(0xec8,128),players=hex(0x480,0x60)}
   invoke(family);coroutine.yield()
   frac=frac+141;camera=camera+frac//256;frac=frac%256
  end
 end end end
end)
local function report(p)
 assert(cpu.state["CURPC"].value==0x226,"Source actor dispatch did not return: "..cpu.state["CURPC"].value)
 f:write(string.format('{"family":%d,"slot":%d,"scenario":%d,"tick":%d,"camera":%d,"floor_x":%d,"floor_y":%d,"before":"%s","after":"%s","weapons_before":"%s","weapons_after":"%s","players":"%s","collision":"%s"}\n',p.family,p.slot,p.scenario,p.tick,p.camera,p.floor_x,p.floor_y,p.before,hex(p.base-16,32),p.weapons,hex(0xec8,128),p.players,hex(0x100,128)))
end
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end
 frame=frame+1;if frame<60 then return end
 if frame==60 then
  floor_tap=m:install_read_tap(0xa847,0xa847,"source-floor-arguments",function()
   if pending then
    assert(pending.floor_x==-1,"Multiple floor queries require an extended fixture")
    local x=cpu.state["X"].value
    pending.floor_x=m:read_u16(x-11);pending.floor_y=m:read_u8(x-8)
   end
  end)
 end
 local ok,err=pcall(function()
  if pending then report(pending);pending=nil end
  if coroutine.status(cases)=="dead"then terminal=true;f:close();print("FOE_ORACLE_DONE",frame);manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
 end)
 if not ok then terminal=true;f:close();print("FOE_ORACLE_ERROR",err);manager.machine:exit()end
end)
