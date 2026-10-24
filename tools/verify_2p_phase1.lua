local mem=manager.machine.devices[":maincpu"].spaces["program"]
local function rs16(a) local v=mem:read_u16(a); if v>=32768 then v=v-65536 end; return v end
local function sb(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Start","P1 A","P1 B","P1 C","P1 Up","P1 Down","P1 Left","P1 Right"}) do sb(n,0) end end
local fr=0
local f=io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/verify_2p_phase1.txt","w")
local function log(tag) f:write(string.format("%-6s fr=%d px=%d cam=%d screenX=%d\n",tag,fr,rs16(0xFF07EC),rs16(0xFF082E),rs16(0xFF07EC)-rs16(0xFF082E))); f:flush() end
emu.register_frame_done(function()
  fr=fr+1; rel()
  if (fr>=720 and fr<732) or (fr>=820 and fr<832) or (fr>=920 and fr<932) then sb("P1 Start",1) end
  if fr>=1100 and fr<1220 then sb("P1 Right",1); if (fr%20)==0 then log("RIGHT") end end
  if fr>=1260 and fr<1380 then if (fr%20)==0 then log("IDLE") end end           -- no input: rides scroll
  if fr>=1420 and fr<1540 then sb("P1 Left",1); if (fr%20)==0 then log("LEFT") end end
  if fr>=1560 then f:close(); manager.machine:exit() end
end)
print("running")
