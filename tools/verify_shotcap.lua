-- verify_shotcap.lua : confirm the faithful 1-shot cap ($0E28 single slot) +
-- that fire-spam no longer drops frames (the old ~30fps cliff). Boots, then SPAMS
-- fire (B/C edge every 4 frames) while running right; tracks the max simultaneous
-- live player shots (ET_SHOT=2) and the fps under spam.
local GFC=0xFF0076; local LEVELW=0xFF078A; local ENTS=0xFF080C; local STRIDE=0x2A; local EMAX=48
local mem; local em=0; local ig; local gfc0; local maxshots=0; local maxactive=0
local function pad(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do pad(n,0) end end
emu.register_frame_done(function()
  if not mem then mem=manager.machine.devices[":maincpu"].spaces["program"] end
  em=em+1; rel()
  local lw=mem:read_u16(LEVELW)
  if lw==0 then if (em%80)<12 then pad("P1 Start",1) end; return end
  if not ig then ig=em; gfc0=mem:read_u32(GFC) end
  local t=em-ig
  pad("P1 Right",1)
  if (t%4) < 2 then pad("P1 B",1); pad("P1 C",1) end   -- spam fire edges
  -- census: count active entities + active ET_SHOT (type==2)
  local nsh, nact = 0, 0
  for i=0,EMAX-1 do
    local b=ENTS+i*STRIDE
    if mem:read_u8(b) ~= 0 then nact=nact+1; if mem:read_u8(b+1)==2 then nsh=nsh+1 end end
  end
  if nsh>maxshots then maxshots=nsh end
  if nact>maxactive then maxactive=nact end
  if (t%120)==0 and t>0 then print(string.format("t=%d shots=%d active=%d gfc=%d", t, nsh, nact, mem:read_u32(GFC))) end
  if t>=420 then
    local gf=mem:read_u32(GFC)-gfc0
    print(string.format("RESULT: MAX simultaneous player shots=%d (faithful=1); max active entities=%d; fps=%.3f game-frames/emu-frame over %d frames of fire-spam (1.000=60)",
          maxshots, maxactive, gf/t, t))
    manager.machine:exit()
  end
end)
print("verify_shotcap.lua running")
