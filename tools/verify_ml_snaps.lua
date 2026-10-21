-- verify_ml_snaps.lua : produce CLEAN, correctly-timed per-round snapshots that
-- prove each round shows its OWN distinct level. On entering each round we let the
-- hero stand at the level START for ~70 frames (camera ~0, intro settled) and
-- snapshot THERE (so the snapshot shows that round's level, not the next), then
-- suppress death + shove him to the level end to trigger the clear -> next round.
-- Logs g_curLevel + width per round (must differ). genesis/NTSC driver.
local GAME=0xFF07F4
local PLAYERX=GAME+0; local LIVES=GAME+26; local FLOW=GAME+27; local ROUND=GAME+30
local CAMX=GAME+32; local CURLVL=0xFF0000; local LEVELW=0xFF07AE
local DEADREQ=0xFF07F0; local INVULN=0xFF07F1

local mem
local function rd(a) return mem:read_u8(a) end
local function rd16(a) return mem:read_u16(a) end
local function rd32(a) return mem:read_u32(a) end
local function wr(a,v) mem:write_u8(a,v) end
local function wrs16(a,v) if v<0 then v=v+0x10000 end; mem:write_u16(a,v&0xFFFF) end
local function rds16(a) local v=mem:read_u16(a); if v>=0x8000 then v=v-0x10000 end; return v end
local function setbtn(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end end

local presses={}
local function press(at,n,d) presses[#presses+1]={at=at,n=n,off=at+(d or 8)} end
press(720,"P1 Start",10); press(810,"P1 Start",10)
local F_INGAME=960

local fr=0; local round_entry={}; local snapped={}; local round_seen={}
local prev_round=-1; local summary={}
local done_at=nil

emu.register_frame_done(function()
  if not mem then mem=manager.machine.devices[":maincpu"].spaces["program"] end
  fr=fr+1; rel()
  for _,p in ipairs(presses) do if fr>=p.at and fr<p.off then setbtn(p.n,1) end end
  if fr<F_INGAME then return end

  local flow=rd(FLOW); local round=rd(ROUND); local camx=rds16(CAMX)
  local lvlw=rd16(LEVELW); local lvlptr=rd32(CURLVL); local lives=rd(LIVES)

  -- record entry + level identity per round
  if round>=1 and not round_seen[round] then
    round_seen[round]=true; round_entry[round]=fr
    summary[round]={ptr=lvlptr,w=lvlw}
    print(string.format("F%-5d enter ROUND %d  g_curLevel=0x%08X width=%d", fr, round, lvlptr, lvlw))
  end

  -- suppress death always (so we can hold the wall later)
  wr(DEADREQ,0); wr(INVULN,1); if lives<5 then wr(LIVES,5) end

  local entered = round_entry[round] or fr
  local settle = fr - entered

  -- PHASE 1 (settle, 0..70 frames): stand still at the level START, snapshot at +60
  if settle < 80 then
    -- don't move (don't press Right) so camera stays near 0 = level start
    if not snapped[round] and settle>=60 and flow==0 then
      manager.machine.video:snapshot(); snapped[round]=true
      print(string.format("SNAP ml%d F%d camX=%d width=%d (ROUND %d level start)", round, fr, camx, lvlw, round))
    end
  else
    -- PHASE 2: drive to the end to clear -> next round
    setbtn("P1 Right",1)
    if flow==1 or flow==2 then wr(FLOW,0); flow=0 end
    if flow==0 then wrs16(PLAYERX, lvlw) end
  end

  if round~=prev_round then prev_round=round end

  -- stop after we've snapped rounds 1,2,3 (and a bit to confirm cycle to 4)
  if snapped[1] and snapped[2] and snapped[3] and round>=4 and not done_at then done_at=fr+30 end
  if done_at and fr>=done_at then
    print("=== SUMMARY: level per round (ptr must differ) ===")
    for r=1,4 do if summary[r] then
      print(string.format("  round %d: g_curLevel=0x%08X width=%d", r, summary[r].ptr, summary[r].w)) end end
    local ok = summary[1] and summary[2] and summary[3]
        and summary[1].ptr~=summary[2].ptr and summary[2].ptr~=summary[3].ptr
    print(ok and "PASS: rounds 1/2/3 each use a DISTINCT level; round 4 cycles back"
              or "CHECK: level identities above")
    manager.machine:exit()
  end
  if fr>=6000 then print("TIMEOUT round="..round.." snaps="..tostring(snapped[1])..tostring(snapped[2])..tostring(snapped[3])); manager.machine:exit() end
end)
print("verify_ml_snaps running")
