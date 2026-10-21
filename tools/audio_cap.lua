-- audio_cap.lua -- drive to gameplay and PLAY (run + fire) for a sustained
-- window so an external WAV capture (-wavwrite) records the BGM (PSG) + fire SFX
-- (YM). Holds RIGHT + mashes B to make the fire SFX repeat over the music.
local function setbtn(name, v)
  local p = manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]
  if p then local fd = p.fields[name]; if fd then fd:set_value(v) end end
end
local function release_all()
  for _, n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end
end
local F_SEL=980; local F_DIP=1180; local F_PLAY=1300; local F_END=2800
local fr=0
emu.register_frame_done(function()
  fr=fr+1
  release_all()
  if fr>=F_SEL and fr<F_SEL+8 then setbtn("P1 Start",1) end
  if fr>=F_DIP and fr<F_DIP+8 then setbtn("P1 Start",1) end
  if fr>=F_PLAY then
    setbtn("P1 Right",1)
    if (fr % 10) < 3 then setbtn("P1 B",1) end   -- fire every ~10f
    if (fr % 300) < 40 then setbtn("P1 Up",1) end -- climb sometimes
  end
  if fr>=F_END then manager.machine:exit() end
end)
emu.print_info("audio_cap running")
