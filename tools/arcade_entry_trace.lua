-- Isolated original-MC6809 routine calls. Source code executes unmodified.
-- A RAM test wrapper JSRs the routine and spins with IRQ/FIRQ masked. The NOP
-- runway avoids replacing a PC in the middle of the previous branch microcycle.
-- This establishes local contracts, not natural game reachability/call order.
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local path=os.getenv("SONSON_ENTRY_TRACE") or "out/parity-audit/entry-oracle-v2.jsonl"
local f=assert(io.open(path,"w"));local frame=0;local pending=nil;local terminal=false
local function hex(a,n)local t={}for i=0,n-1 do t[#t+1]=string.format("%02x",m:read_u8(a+i))end;return table.concat(t)end
local function output(s)f:write(s.."\n")end
local function invoke(pc)
 for a=0xf00,0xf1f do m:write_u8(a,0x12)end
 m:write_u8(0xf20,0x8e);m:write_u16(0xf21,cpu.state["X"].value);m:write_u8(0xf23,0xbd);m:write_u16(0xf24,pc);m:write_u8(0xf26,0x20);m:write_u8(0xf27,0xfe)
 cpu.state["S"].value=0xff0;cpu.state["DP"].value=0;cpu.state["CC"].value=0x50;cpu.state["PC"].value=0xf00
end
local function clear(a,n)for i=0,n-1 do m:write_u8(a+i,0)end end

local cases=coroutine.create(function()
 for family=0,1 do for facing=1,3,2 do for _,delay in ipairs({0,1,8,22,80})do for _,fraction in ipairs({0,255})do for stopped=0,1 do
  local base=family==0 and 0xc28 or 0xce8;clear(0xc18,0x180);m:write_u8(0xa1,0)
  local camera=0x2345;local camera_fraction=0
  m:write_u8(base-16,0x80);m:write_u16(base-11,camera+(facing==1 and -8 or 248));m:write_u8(base-9,fraction);m:write_u8(base-8,0x88);m:write_u8(base-6,facing);m:write_u8(base+13,delay)
  m:write_u8(0x9d,stopped);m:write_u8(0xaa+family,0)
  for tick=0,200 do
   m:write_u16(0x90,camera);cpu.state["X"].value=base
   local state=m:read_u8(base-15)
   assert(state<2)
   local pc=family==0 and 0xc69b or 0xcefa
   local before=hex(base-16,32);invoke(pc)
   pending={family=family,facing=facing,delay=delay,fraction=fraction,stopped=stopped,tick=tick,base=base,camera=camera,before=before};coroutine.yield()
   if m:read_u8(base-15)==2 then break end
   assert(tick<200,"Entry did not reach ordinary AI")
   if stopped==0 then camera_fraction=camera_fraction+141;camera=camera+(camera_fraction//256);camera_fraction=camera_fraction%256 end
  end
 end end end end end
end)
local function report(p)
 assert(cpu.state["CURPC"].value==0xf26,"Entry routine did not return")
 output(string.format('{"family":%d,"facing":%d,"delay":%d,"fraction":%d,"stopped":%d,"tick":%d,"camera":%d,"before":"%s","after":"%s"}',p.family,p.facing,p.delay,p.fraction,p.stopped,p.tick,p.camera,p.before,hex(p.base-16,32)))
end
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end
 frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then report(pending);pending=nil end
  if coroutine.status(cases)=="dead"then terminal=true;f:close();print("ENTRY_ORACLE_DONE",frame,path);manager.machine:exit();return end
  local resumed,e=coroutine.resume(cases);assert(resumed,e)
  assert(frame<20000,"Source entry fixture timeout")
 end)
 if not ok then terminal=true;f:close();print("ENTRY_ORACLE_ERROR",err);manager.machine:exit()end
end)
