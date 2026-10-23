-- Isolated original-MC6809 routine calls. Source code executes unmodified.
-- A RAM test wrapper JSRs the routine and spins with IRQ/FIRQ masked. The NOP
-- runway avoids replacing a PC in the middle of the previous branch microcycle.
-- This establishes local contracts, not natural game reachability/call order.
local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local path=os.getenv("SONSON_WAVE_TRACE") or "out/parity-audit/wave-oracle.jsonl"
local f=assert(io.open(path,"w"));local frame=0;local pending=nil
local function hex(a,n)local t={}for i=0,n-1 do t[#t+1]=string.format("%02x",m:read_u8(a+i))end;return table.concat(t)end
local function output(s)f:write(s.."\n")end
local function invoke(pc)
 for a=0xf00,0xf1f do m:write_u8(a,0x12)end
 m:write_u8(0xf20,0xbd);m:write_u16(0xf21,pc);m:write_u8(0xf23,0x20);m:write_u8(0xf24,0xfe)
 cpu.state["S"].value=0xff0;cpu.state["DP"].value=0;cpu.state["CC"].value=0x50;cpu.state["PC"].value=0xf00
end
local function clear(a,n)for i=0,n-1 do m:write_u8(a+i,0)end end
local cases=coroutine.create(function()
 -- Every input to the 16-bit source RNG, not a self-derived expected table.
 for seed=0,65535 do
  m:write_u16(0x78,seed);invoke(0x95f4)
  pending={kind="rng",seed=seed};coroutine.yield()
 end
 -- One source wave load followed by its twelve consumer ticks. Existing active
 -- controllers retain their state; absent bits clear only active/state/phase.
 for wave=0,86 do
  clear(0x628,0xd0);m:write_u8(0x628,255);m:write_u8(0x90,wave);m:write_u8(0x9c,0)
  for k=0,11 do local a=0x638+k*16
   m:write_u8(a,k%2);m:write_u8(a+1,7);m:write_u8(a+2,2);m:write_u8(a+3,3);m:write_u16(a+10,0x1234);m:write_u8(a+14,0xa5)
  end
  for tick=0,12 do invoke(0xef7c);pending={kind="wave",wave=wave,tick=tick};coroutine.yield()end
 end
 for delay=1,3 do for _,phase in ipairs({0,1,63,64,255})do
  clear(0x628,0xd0);m:write_u8(0x9c,delay);m:write_u8(0x24,phase)
  invoke(0xef7c);pending={kind="delay",delay=delay,clock=phase};coroutine.yield()
 end end
 for family=0,1 do for group=0,3 do for variant=0,3 do
  for _,parameter in ipairs({0,1,2,0x81,0x82,0xff})do
   for _,phase in ipairs({0,29,30,31,255})do for live=0,1 do for statephase=0,3 do
    local c=(family==0)and 0x698 or 0x688
    clear(c,16);clear(0xc18,0x180)
    m:write_u8(c,1);m:write_u8(c+1,1);m:write_u8(c+2,statephase==3 and 1 or 0);m:write_u8(c+3,statephase==3 and 0 or statephase);m:write_u8(c+14,parameter);m:write_u16(c+10,0x1234)
    m:write_u8(0xaa+family,live);m:write_u8(0xb2+family,9)
    m:write_u8(0x24,phase);m:write_u8(0x96,group*16);m:write_u16(0x90,0x2345)
    local seed=({0x4000,0,0x4001,1})[variant+1];m:write_u16(0x78,seed)
    m:write_u8(0x480,1);m:write_u8(0x483,0);m:write_u16(0x485,0x23f5);m:write_u8(0x4b0,0);m:write_u8(0x99,0)
    invoke(family==0 and 0xf682 or 0xf7f3)
    pending={kind="formation",family=family,group=group,variant=variant,parameter=parameter,clock=phase,live=live,statephase=statephase,seed=seed};coroutine.yield()
   end end end
  end
 end end end
end)
local function report(p)
 assert(cpu.state["CURPC"].value==0xf23,"Source routine failed to return: "..cpu.state["CURPC"].value)
 if p.kind=="rng"then
  output(string.format('{"kind":"rng","seed":%d,"next":%d,"last":%d}',p.seed,m:read_u16(0x78),m:read_u8(0x7a)))
 elseif p.kind=="wave"then
  output(string.format('{"kind":"wave","wave":%d,"tick":%d,"producer":"%s","controls":"%s"}',p.wave,p.tick,hex(0x628,16),hex(0x638,192)))
 elseif p.kind=="delay"then
  output(string.format('{"kind":"delay","delay":%d,"clock":%d,"after":%d}',p.delay,p.clock,m:read_u8(0x9c)))
 else
  local c=p.family==0 and 0x698 or 0x688;local base=p.family==0 and 0xc18 or 0xcd8
  output(string.format('{"kind":"formation","family":%d,"group":%d,"variant":%d,"parameter":%d,"clock":%d,"live":%d,"statephase":%d,"seed":%d,"next":%d,"last":%d,"reservation":%d,"control":"%s","actors":"%s"}',p.family,p.group,p.variant,p.parameter,p.clock,p.live,p.statephase,p.seed,m:read_u16(0x78),m:read_u8(0x7a),m:read_u8(0xb2+p.family),hex(c,16),hex(base,192)))
 end
end
emu.register_frame_done(function()
 frame=frame+1
 if frame<60 then return end
 if pending then report(pending);pending=nil end
 if coroutine.status(cases)=="dead"then f:close();print("WAVE_ORACLE_DONE",frame,path);manager.machine:exit();return end
 local ok,err=coroutine.resume(cases);assert(ok,err)
end)
