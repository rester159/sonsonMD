local FC=0xFF007C; local LW=0xFF0072; local GG=0xFF0808; local EN=0xFF0834
local mem=manager.machine.devices[":maincpu"].spaces["program"]
local function r32(a) return mem:read_u32(a) end
local function r16(a) return mem:read_u16(a) end
local function r8(a) return mem:read_u8(a) end
local function wr8(a,v) mem:write_u8(a,v) end
local function sb(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 B"}) do sb(n,0) end end
local f=io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/fps_check.txt","w")
local fr=0; local g0=nil; local f0=nil; local snapped=false
emu.register_frame_done(function()
  fr=fr+1; rel()
  if r16(LW)==0 then if (fr%16)<8 then sb("P1 Start",1) end return end
  wr8(0xFF0826,5); sb("P1 Right",1)
  if (fr%18)<2 then sb("P1 B",1) end
  local ph=(fr//40)%4; if ph==1 then sb("P1 Up",1) elseif ph==3 then sb("P1 Down",1) end
  if fr==1200 then f0=fr; g0=r32(FC) end
  if fr==2400 and g0 then local ratio=(r32(FC)-g0)/(fr-f0); f:write(string.format("fps ratio=%.4f\n",ratio)) end
  -- count creatures + snapshot a busy frame
  if fr==2000 and not snapped then manager.machine.video:snapshot(); snapped=true; f:write("SNAP fr2000\n") end
  if fr>=2500 then f:close(); manager.machine:exit() end
end)
print("running")
