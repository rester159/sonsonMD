-- audio_nofire.lua -- drive to gameplay and run RIGHT but NEVER fire, so the
-- capture contains ONLY the BGM (PSG, cmd 0x18). A/B vs audio_cap (with fire)
-- isolates the fire SFX contribution + proves the music plays on its own.
local function setbtn(name, v)
  local p = manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]
  if p then local fd = p.fields[name]; if fd then fd:set_value(v) end end
end
local function release_all()
  for _, n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end
end
local F_SEL=980; local F_DIP=1180; local F_PLAY=1300; local F_END=2400
local fr=0
emu.register_frame_done(function()
  fr=fr+1
  release_all()
  if fr>=F_SEL and fr<F_SEL+8 then setbtn("P1 Start",1) end
  if fr>=F_DIP and fr<F_DIP+8 then setbtn("P1 Start",1) end
  if fr>=F_PLAY then setbtn("P1 Right",1) end   -- run, but NO fire
  if fr>=F_END then manager.machine:exit() end
end)
emu.print_info("audio_nofire running")
