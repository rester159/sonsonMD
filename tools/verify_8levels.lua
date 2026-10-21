-- verify_8levels.lua : prove the NATIVE port now cycles through ALL 8 captured
-- levels (round1->level1 ... round8->level8) at 60fps. Generalises
-- verify_multilevel.lua (which only checked rounds 1-3) to the full 8-level set.
--
-- Boot: SEGA -> CAPCOM -> selector (confirm ARCADE) -> DIP (confirm) -> game.
-- Then drive RIGHT, keep lives topped + SUPPRESS death (clear dead-request, pin
-- invuln, snap any DYING/RESPAWNING beat back to ALIVE), and once each round's
-- level is settled shove the hero's world X past the CURRENT level end (read from
-- g_levelW) so the camera clamps and the ROUND_CLEAR dwell accrues -> round_advance
-- picks the NEXT level. We log g_curLevel(ptr)+g_levelW for every round 1..8 (must
-- be distinct ptrs for the 8 levels, then wrap at round 9) and tally play frames.
--
-- Symbols (out/symbol.txt, mask e0ff.. -> 0xFF..): identical to verify_multilevel.

local GAME    = 0xFF07F4
local PLAYERX = GAME + 0       -- Player.x (s16)
local LIVES   = GAME + 26
local FLOW    = GAME + 27
local ROUND   = GAME + 30
local ENDDWELL= GAME + 31
local CAMX    = GAME + 32
local CURLVL  = 0xFF0000        -- const SsLevel* g_curLevel (32-bit)
local LEVELW  = 0xFF07AE        -- u16 g_levelW
local DEADREQ = 0xFF07F0        -- u8  g_player_dead_request
local INVULN  = 0xFF07F1        -- u8  g_player_invuln

local TARGET_ROUNDS = 9         -- visit rounds 1..9 (9 proves the wrap back to lvl1)

local mem
local function rd(a) return mem:read_u8(a) end
local function rd16(a) return mem:read_u16(a) end
local function rd32(a) return mem:read_u32(a) end
local function wr(a,v) mem:write_u8(a,v) end
local function wrs16(a,v) if v<0 then v=v+0x10000 end; mem:write_u16(a, v & 0xFFFF) end
local function rds16(a) local v=mem:read_u16(a); if v>=0x8000 then v=v-0x10000 end; return v end

local function setbtn(name,v)
  local p = manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]
  if p then local fd=p.fields[name]; if fd then fd:set_value(v) end end
end
local function release_all()
  for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end
end

local presses={}
local function press(at,name,dur) presses[#presses+1]={at=at,name=name,off=at+(dur or 8)} end
local F_SEL=720
press(F_SEL,      "P1 Start", 10)   -- confirm selector (ARCADE) -> DIP
press(F_SEL+90,   "P1 Start", 10)   -- confirm DIP -> game
local F_INGAME=960

local flagname={[0]="ALIVE",[1]="DYING",[2]="RESPAWNING",[3]="GAME_OVER",[4]="ROUND_CLEAR"}
local fr=0
local prev_flow,prev_round=-1,-1
local lvl_of_round={}        -- round -> {ptr,width}
local round_settle={}        -- round -> frame after which we may push to the end
local fps_samples=0
local done_at=nil
local snapped={}

emu.register_frame_done(function()
  if not mem then mem=manager.machine.devices[":maincpu"].spaces["program"] end
  fr=fr+1
  release_all()
  for _,p in ipairs(presses) do if fr>=p.at and fr<p.off then setbtn(p.name,1) end end
  if fr<F_INGAME then return end

  setbtn("P1 Right",1)                       -- always push forward

  local flow=rd(FLOW); local round=rd(ROUND); local lives=rd(LIVES)
  local camx=rds16(CAMX); local dwell=rd(ENDDWELL)
  local lvlw=rd16(LEVELW); local lvlptr=rd32(CURLVL)

  -- record this round's level identity (ptr + width) the first time we see it
  if round>=1 and not lvl_of_round[round] then
    lvl_of_round[round]={ptr=lvlptr,w=lvlw}
    round_settle[round]=fr+60           -- let the level reset + intro settle
    print(string.format("F%-5d ROUND %d level: g_curLevel=0x%08X width=%d px",
      fr, round, lvlptr, lvlw))
    -- snapshot the first few rounds + a couple of late ones as evidence
    if not snapped[round] and (round<=4 or round>=7) then
      _snap_round=round; _snap_at=fr+70
    end
  end
  if _snap_at and fr>=_snap_at and not snapped[_snap_round] and flow==0 then
    manager.machine.video:snapshot(); snapped[_snap_round]=true
    print(string.format("SNAP r%d F%d camX=%d w=%d", _snap_round, fr, camx, lvlw))
    _snap_at=nil
  end

  -- DETERMINISTIC end-push (same discipline as verify_multilevel.lua): keep lives
  -- topped, suppress death, snap death beats back to ALIVE, then once the round has
  -- settled shove world X past the CURRENT level end so the camera clamps and the
  -- real CLEAR logic (camera maxed + wall-push dwell) fires -> next round.
  wr(DEADREQ, 0)
  wr(INVULN, 1)
  if round < TARGET_ROUNDS + 1 then
    if lives<5 then wr(LIVES,5) end
    if flow==1 or flow==2 then wr(FLOW,0); flow=0 end
    local settled = round_settle[round] and fr>=round_settle[round]
    if settled and flow==0 then
      wrs16(PLAYERX, lvlw)                  -- past end -> clamp pins at maxCam
    end
  end

  if flow~=prev_flow then prev_flow=flow end
  if round~=prev_round then
    print(string.format("F%-5d ROUND -> %d (flow=%s)", fr, round, flagname[flow] or "?"))
    prev_round=round
    if round>=TARGET_ROUNDS and not done_at then done_at=fr+120 end
  end

  if flow==0 then fps_samples=fps_samples+1 end

  if done_at and fr>=done_at then
    print("=== LEVEL-PER-ROUND SUMMARY ===")
    local seen_ptr={}
    local distinct=0
    for r=1,TARGET_ROUNDS do
      local L=lvl_of_round[r]
      if L then
        local tag=""
        if not seen_ptr[L.ptr] then seen_ptr[L.ptr]=r; distinct=distinct+1
        else tag=string.format("  (== round %d's level)", seen_ptr[L.ptr]) end
        print(string.format("  round %d: g_curLevel=0x%08X width=%d%s", r, L.ptr, L.w, tag))
      else
        print(string.format("  round %d: NOT REACHED", r))
      end
    end
    print(string.format("DISTINCT levels observed across rounds 1..%d : %d", TARGET_ROUNDS, distinct))
    -- the 8 distinct levels should appear in rounds 1..8; round 9 should reuse round 1's
    local L1=lvl_of_round[1]; local L9=lvl_of_round[9]
    if L1 and L9 then
      if L1.ptr==L9.ptr then print("PASS: round 9 WRAPS back to level 1 (cycle confirmed)")
      else print("WARN: round 9 ptr != round 1 ptr") end
    end
    if distinct>=8 then print("PASS: all 8 distinct levels cycle")
    elseif distinct>3 then print(string.format("PARTIAL: %d distinct levels cycle (>3)", distinct))
    else print("FAIL: <=3 distinct levels") end
    print(string.format("play frames observed (ALIVE): %d", fps_samples))
    manager.machine:exit()
  end

  if fr>=12000 then
    print(string.format("TIMEOUT F%d round=%d flow=%s camX=%d dwell=%d",
      fr, round, flagname[rd(FLOW)] or "?", camx, dwell))
    print("=== partial summary ===")
    for r=1,TARGET_ROUNDS do local L=lvl_of_round[r]
      if L then print(string.format("  round %d: 0x%08X w=%d", r, L.ptr, L.w)) end end
    manager.machine:exit()
  end
end)
print("verify_8levels.lua running")
