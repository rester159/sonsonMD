local GLEVELW=0xFF0072; local mem=manager.machine.devices[":maincpu"].spaces["program"]
local function r16(a) return mem:read_u16(a) end
local function sb(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do sb(n,0) end end
local f=io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/verify_title.txt","w")
local fr=0; local shot=0
emu.register_frame_done(function()
  fr=fr+1; rel()
  -- pulse Start every 150 frames (advance selector->config->title->game)
  if (fr%150)<5 then sb("P1 Start",1) end
  -- snapshot mid-cycle (75 after each pulse) once past the SEGA splash
  if fr>=450 and (fr%150)==75 and shot<14 then
    manager.machine.video:snapshot()
    f:write(string.format("snap %02d fr=%d levelW=%d\n",shot,fr,r16(GLEVELW))); f:flush(); shot=shot+1
  end
  if fr>=2700 then f:close(); manager.machine:exit() end
end)
print("running")
