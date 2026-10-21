-- seam_snap.lua : LIVE seam-tear check for the seamless ribbon (the agent could
-- not snapshot headless). Top lives, hold RIGHT through all 8 level seams + the
-- loop-wrap (seg 7->0), snapshot at each segment change, track 60fps.
local LEVELW=0xFF0072; local IDX=0xFF0075; local GFC=0xFF007A; local LIVES=0xFF07FA
local mem; local em=0; local ig; local gfc0; local previdx=-1; local looped=false; local maxidx=0
local function pad(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do pad(n,0) end end
emu.register_frame_done(function()
  if not mem then mem=manager.machine.devices[":maincpu"].spaces["program"] end
  em=em+1; rel()
  local lw=mem:read_u16(LEVELW)
  if lw==0 then if (em%80)<12 then pad("P1 Start",1) end; return end
  if not ig then ig=em; gfc0=mem:read_u32(GFC); print("INGAME em="..em.." ribbonW="..lw) end
  if mem:read_u8(LIVES) < 5 then mem:write_u8(LIVES, 5) end   -- survive the whole loop
  pad("P1 Right",1)
  local idx = mem:read_u8(IDX)
  if idx ~= previdx then
    if previdx >= 0 then
      print(string.format("SEAM %d->%d  em=%d  gfc=%d", previdx, idx, em, mem:read_u32(GFC)))
      manager.machine.video:snapshot()
      if previdx==7 and idx==0 then looped=true; print(">>> LOOP-WRAP seam (7->0) captured <<<") end
    end
    if idx>maxidx then maxidx=idx end
    previdx = idx
  end
  if looped or (em-ig) >= 8000 then
    local gf=mem:read_u32(GFC)-gfc0; local ef=em-ig
    print(string.format("DONE: maxSeg=%d looped=%s  fps=%.4f game-frames/emu-frame over %d frames (1.0=60)",
          maxidx, tostring(looped), gf/ef, ef))
    manager.machine:exit()
  end
end)
print("seam_snap.lua running")
