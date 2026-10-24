local mem=manager.machine.devices[":maincpu"].spaces["program"]
local function rs16(a) local v=mem:read_u16(a); if v>=32768 then v=v-65536 end; return v end
local function p1(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function p2(n,v) local p=manager.machine.ioport.ports[":ctrl2:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel()
  for _,n in ipairs({"P1 Start","P1 Up","P1 Down","P1 Left","P1 Right"}) do p1(n,0) end
  for _,n in ipairs({"P2 Up","P2 Down","P2 Left","P2 Right"}) do p2(n,0) end
end
local fr=0
local f=io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/verify_2p_phase2.txt","w")
local function log(t)
  local cam=rs16(0xFF083A)
  f:write(string.format("%-9s fr=%d cam=%d  P1sx=%d  P2sx=%d\n",t,fr,cam,rs16(0xFF07F8)-cam,rs16(0xFF0812)-cam)); f:flush()
end
emu.register_frame_done(function()
  fr=fr+1; rel()
  if fr<1000 then mem:write_u8(0xFF0000,2) end
  if (fr>=720 and fr<732) or (fr>=820 and fr<832) or (fr>=920 and fr<932) then p1("P1 Start",1) end
  -- A: only P1 RIGHT (P2 idle)
  if fr>=1080 and fr<1180 then p1("P1 Right",1); if (fr%30)==0 then log("P1-RIGHT") end end
  -- B: only P2 RIGHT (P1 idle)
  if fr>=1230 and fr<1330 then p2("P2 Right",1); if (fr%30)==0 then log("P2-RIGHT") end end
  -- C: only P1 LEFT (P2 idle)
  if fr>=1380 and fr<1480 then p1("P1 Left",1); if (fr%30)==0 then log("P1-LEFT") end end
  if fr==1300 then manager.machine.video:snapshot() end
  if fr>=1500 then f:close(); manager.machine:exit() end
end)
print("running")
