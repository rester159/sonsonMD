-- verify_8levels_fps.lua : prove the game holds 60fps WHILE PLAYING A LATER ROUND
-- (so one of the newly-captured levels 4-8 is the resident/streaming level), with
-- a busy scene (run + shots). Measures the ROM's free-running game-loop tick
-- g_frameCounter across a known emulated-frame span: 1:1 with emu frames => 60fps.
--
-- Uses the SAME deterministic front-end nav + end-push as verify_8levels.lua to
-- fast-forward to round 5, then settles into a busy scene there and samples the
-- tick. genesis driver = USA NTSC 60Hz (correct timebase).
--
-- IMPORTANT: g_frameCounter address is read fresh from this build (it drifts as
-- data layout changes -- the old entity_fps.lua hard-coded a now-stale addr).

local FRAMECTR = 0xFF0076      -- g_frameCounter (u32) -- from out/symbol.txt THIS build
local GAME    = 0xFF07F4
local PLAYERX = GAME + 0
local LIVES   = GAME + 26
local FLOW    = GAME + 27
local ROUND   = GAME + 30
local LEVELW  = 0xFF07AE
local DEADREQ = 0xFF07F0
local INVULN  = 0xFF07F1

local PLAY_ROUND = 5           -- measure FPS while this round (a NEW level) is live

local mem
local function rd(a) return mem:read_u8(a) end
local function rd16(a) return mem:read_u16(a) end
local function rd32(a) return mem:read_u32(a) end
local function wr(a,v) mem:write_u8(a,v) end
local function wrs16(a,v) if v<0 then v=v+0x10000 end; mem:write_u16(a, v & 0xFFFF) end

local function setbtn(name,v)
  local p = manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]
  if p then local fd=p.fields[name]; if fd then fd:set_value(v) end end
end
local function release_all()
  for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end
end

local presses={}
local function press(at,name,dur) presses[#presses+1]={at=at,name=name,off=at+(dur or 8)} end
press(720, "P1 Start", 10)     -- confirm selector
press(810, "P1 Start", 10)     -- confirm DIP -> game
local F_INGAME=960

local fr=0
local round_settle={}
local seen_round={}
local m0,m1,measureStart=nil,nil,nil
local SPAN=300                  -- emulated frames to measure over

emu.register_frame_done(function()
  if not mem then mem=manager.machine.devices[":maincpu"].spaces["program"] end
  fr=fr+1
  release_all()
  for _,p in ipairs(presses) do if fr>=p.at and fr<p.off then setbtn(p.name,1) end end
  if fr<F_INGAME then return end

  local flow=rd(FLOW); local round=rd(ROUND); local lives=rd(LIVES); local lvlw=rd16(LEVELW)

  if not seen_round[round] then seen_round[round]=true; round_settle[round]=fr+60
    print(string.format("F%-5d reached round %d (w=%d, gtick=%d)", fr, round, lvlw, rd32(FRAMECTR))) end

  -- suppress death + keep lives topped
  wr(DEADREQ,0); wr(INVULN,1)
  if lives<5 then wr(LIVES,5) end
  if flow==1 or flow==2 then wr(FLOW,0); flow=0 end

  if round < PLAY_ROUND then
    -- fast-forward: shove to the level end so the round clears, advancing levels
    local settled = round_settle[round] and fr>=round_settle[round]
    if settled and flow==0 then wrs16(PLAYERX, lvlw) end
    setbtn("P1 Right",1)
  else
    -- on the target round: BUSY scene -- run forward + mash B for a shot stream,
    -- but DON'T shove to the end (we want to stay in this level while measuring).
    setbtn("P1 Right",1)
    if (fr % 8) < 3 then setbtn("P1 B",1) end

    if not measureStart and flow==0 and round_settle[round] and fr>=round_settle[round] then
      measureStart=fr+30                       -- let the busy scene populate
    end
    if measureStart and fr==measureStart then
      m0=rd32(FRAMECTR)
      print(string.format("SAMPLE0 emuF=%d gtick=%d round=%d w=%d", fr, m0, round, lvlw))
    end
    if measureStart and fr==measureStart+SPAN then
      m1=rd32(FRAMECTR)
      local gameSpan=m1-m0
      local fps=60.0*gameSpan/SPAN
      print(string.format("SAMPLE1 emuF=%d gtick=%d", fr, m1))
      print(string.format("RESULT round=%d  game-frames=%d over emu-frames=%d => FPS=%.2f",
        round, gameSpan, SPAN, fps))
      if fps>=58.0 then print("PASS: 60fps holds on a NEW level (round "..round..")")
      else print(string.format("WARN: %.2f fps < 58 on round %d", fps, round)) end
      manager.machine.video:snapshot()
      print("SNAP busy_scene_round"..round.." F"..fr)
      manager.machine:exit()
    end
  end

  if fr>=10000 then print("TIMEOUT round="..round.." flow="..flow); manager.machine:exit() end
end)
print("verify_8levels_fps.lua running (FPS on round "..PLAY_ROUND..")")
