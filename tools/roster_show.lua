local GLEVELW=0xFF0072; local GGAME=0xFF0808; local mem=manager.machine.devices[":maincpu"].spaces["program"]
local function r16(a) return mem:read_u16(a) end; local function wr8(a,v) mem:write_u8(a,v) end
local function sb(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 B"}) do sb(n,0) end end
local fr=0
emu.register_frame_done(function()
  fr=fr+1; rel()
  if r16(GLEVELW)==0 then if (fr%16)<8 then sb("P1 Start",1) end return end
  wr8(0xFF0826,5); sb("P1 Right",1)
  if (fr%14)<2 then sb("P1 B",1) end
  local ph=(fr//40)%4; if ph==1 then sb("P1 Up",1) elseif ph==3 then sb("P1 Down",1) end
  if fr==2600 or fr==4200 or fr==5800 then manager.machine.video:snapshot() end
  if fr>=6000 then manager.machine:exit() end
end)
print("running")
