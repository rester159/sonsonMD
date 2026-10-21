-- verify_1up.lua -- confirm the LAYERED extra-life (1-up, cmd 0x04) cue:
--   * fires (dedicated rp_1up channel arms) when the score crosses a bonus threshold;
--   * the BGM (chip-B music slot rp[1][0]) base pointer is UNCHANGED across the event
--     (i.e. the music is NOT replaced -- the whole point of the fix);
--   * YM ch3-5 (the 1-up's private ay[2] file) actually opens its mixer (jingle audible);
--   * 60fps (g_frameCounter / in-game emu frames ~= 1.0).
--
--   mame.exe genesis -cart out/rom.bin -nothrottle -video none -seconds_to_run N \
--     -autoboot_script tools/verify_1up.lua -window -nomaximize
-- env: SS_END (emu frames), SS_POKE (score to poke just below 20000 default 19990)

local mac = manager.machine
local mem = mac.devices[":maincpu"].spaces["program"]

-- symbols from out/symbol.txt (this build)
local A_FRAME = 0xFF007A   -- g_frameCounter (u32)
local A_LEVELW= 0xFF0072   -- g_levelW (0 == front-end)
local A_GAME  = 0xFF0818   -- g_game
local A_RP    = 0xFF07D8   -- rp[2][2] ReplayCh
local A_RP1UP = 0xFF079A   -- rp_1up ReplayCh (dedicated 1-up channel)
local A_AY    = 0xFF07A8   -- ay[3][16]

-- GameState offsets (Player=26): score@26 lives@30 bonusNextIdx@31
local O_SCORE=26; local O_LIVES=30; local O_BIDX=31

local function r8(a) return mem:read_u8(a) end
local function r16(a) return mem:read_u16(a) end
local function r32(a) return mem:read_u32(a) end

-- ReplayCh: base@0(ptr32) pos@4 wait@8(u16) active@10(u8); stride 12
local function rp_base(chip,slot) return r32(A_RP + (chip*2+slot)*12) end
local function rp1up_active() return r8(A_RP1UP + 10) end
local function rp1up_base()   return r32(A_RP1UP) end
-- ay[2] = the 1-up's private file: base + 2*16
local function ay2(i) return r8(A_AY + 32 + i) end

local function score() return r32(A_GAME+O_SCORE) end
local function lives() return r8(A_GAME+O_LIVES) end
local function bidx()  return r8(A_GAME+O_BIDX) end

local PAD = mac.ioport.ports[":ctrl1:mdpad:PAD"]
local function pf(n) if PAD then return PAD.fields[n] end return nil end
local Fstart,Fright,Fup,Fb = pf("P1 Start"),pf("P1 Right"),pf("P1 Up"),pf("P1 B")
local function set(f,v) if f then f:set_value(v) end end
local function rel() set(Fstart,0); set(Fright,0); set(Fup,0); set(Fb,0) end

local end_fr = tonumber(os.getenv("SS_END")) or 2600
local POKE   = tonumber(os.getenv("SS_POKE")) or 19990

local fr=0; local fr0=nil; local poked=false; local pokeFr=nil
local crossed=false; local crossFr=nil; local bgmBaseBeforeCross=nil
local bgmBaseAtPoke=nil; local bgmChanged=false
local oneup_armed_fr=nil; local oneup_audible_fr=nil
local livesBefore=nil; local livesAfter=nil
local bidxBefore=nil
local log={}
local function L(s) log[#log+1]=s; emu.print_info(s) end

emu.register_frame_done(function()
  fr=fr+1; rel()

  if r16(A_LEVELW)==0 then
    if (fr%20)<8 then set(Fstart,1) end          -- pulse START through the front-end
  else
    if not fr0 then fr0=fr; L(string.format(">>> IN-GAME fr=%d",fr)) end
    -- Keep the hero ALIVE so the music slot only changes if the 1-up wrongly
    -- replaces it (no death -> no legitimate 0x14 death-music takeover). Fire to
    -- clear approaching enemies; do NOT charge forward into them.
    if (fr%4)<2 then set(Fb,1) end

    -- Stage 1: settle into steady gameplay, snapshot the BGM, poke score below thr.
    if not poked and (fr-fr0)>240 then
      mem:write_u32(A_GAME+O_SCORE, POKE)
      poked=true; pokeFr=fr
      livesBefore=lives(); bidxBefore=bidx()
      bgmBaseAtPoke=rp_base(1,0)
      L(string.format(">>> POKED score=%d (read back=%d) lives=%d bidx=%d  BGM(slotB)base=0x%X",
                       POKE, score(), livesBefore, bidxBefore, bgmBaseAtPoke))
    end
    -- Stage 2: a few frames later, push the score ACROSS the threshold directly so
    -- check_extra_life() awards the life + plays the 1-up -- deterministic, no death.
    if poked and not crossed and (fr-pokeFr)>30 then
      bgmBaseBeforeCross=rp_base(1,0)
      mem:write_u32(A_GAME+O_SCORE, POKE+200)   -- across the 20000 boundary
      crossed=true; crossFr=fr
      L(string.format(">>> CROSSED score->%d at fr=%d  BGM(slotB)base=0x%X", score(), fr, bgmBaseBeforeCross))
    end

    if crossed then
      -- watch the BGM music slot AFTER the crossing: it must stay the SAME stream
      -- (the 1-up must NOT replace it). Compare to the base captured at the cross.
      local bnow = rp_base(1,0)
      if bgmBaseBeforeCross and bnow~=bgmBaseBeforeCross and not bgmChanged then
        bgmChanged=true
        L(string.format("!!! BGM slotB base CHANGED 0x%X -> 0x%X at fr=%d (%d fr after cross, score=%d)",
                          bgmBaseBeforeCross, bnow, fr, fr-crossFr, score()))
      end
      -- watch the dedicated 1-up channel arm
      if rp1up_active()==1 and not oneup_armed_fr then
        oneup_armed_fr=fr; livesAfter=lives()
        L(string.format(">>> 1-UP CHANNEL ARMED fr=%d base=0x%X  score=%d lives=%d bidx=%d  (BGM base still 0x%X)",
                         fr, rp1up_base(), score(), lives(), bidx(), rp_base(1,0)))
      end
      -- 1-up audible = its private mixer (ay[2][7]) has any tone channel enabled (bit==0)
      if oneup_armed_fr and not oneup_audible_fr then
        local m=ay2(7)
        if (m & 0x07) ~= 0x07 then  -- at least one of tone ch0-2 enabled
          oneup_audible_fr=fr
          L(string.format(">>> 1-UP AUDIBLE fr=%d ay2.mixer=0x%02X vols=%d,%d,%d",
                           fr, m, ay2(8), ay2(9), ay2(10)))
        end
      end
    end
  end

  if fr>=end_fr then
    L("==== SUMMARY ====")
    L(string.format("  poked score=%d -> final score=%d", POKE, score()))
    L(string.format("  lives before=%s after-arm=%s final=%d  bidx before=%s final=%d",
                    tostring(livesBefore), tostring(livesAfter), lives(), tostring(bidxBefore), bidx()))
    L(string.format("  1-up channel armed at fr=%s ; audible at fr=%s",
                    tostring(oneup_armed_fr), tostring(oneup_audible_fr)))
    L(string.format("  BGM(slotB) base before-cross=0x%X final=0x%X  CHANGED-after-cross=%s",
                    bgmBaseBeforeCross or 0, rp_base(1,0), tostring(bgmChanged)))
    local gf=r32(A_FRAME)
    if fr0 then
      local ing=fr-fr0
      L(string.format("  60fps: g_frameCounter=%d over %d in-game frames -> ratio=%.4f", gf, ing, ing>0 and gf/ing or 0))
    end
    local pass = (oneup_armed_fr~=nil) and (oneup_audible_fr~=nil) and (not bgmChanged)
                 and (livesAfter~=nil and livesBefore~=nil and livesAfter>livesBefore)
    L("  RESULT: "..(pass and "PASS (1-up layered, BGM intact, life awarded)" or "CHECK (see above)"))
    mac:exit()
  end
end)
emu.print_info("verify_1up running")
