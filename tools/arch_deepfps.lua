-- arch_deepfps.lua : drive the native port through MULTIPLE round clears (pure
-- hold-RIGHT into each level-end wall so the round-clear dwell fires and advances)
-- to reach the DEEP (round>=4, flyer-dominant) wave program, then measure WORST-
-- CASE deterministic FPS during the busiest scrolling there. No game RAM written.
-- Symbols: g_frameCounter@0xFF0076, g_game@0xFF0840 (round@+0x1E, cam@+0x20),
--          g_entities@0xFF0866 stride 0x2A (fromSched@+0x1C).
local FRAMECTR=0xFF0076; local GGAME=0xFF0840; local ENTITIES=0xFF0866
local EMAX,STRIDE=48,0x2A; local OFF_FROMSCHED,ROUND_OFF,CAM_OFF=0x1C,0x1E,0x20
local mem=manager.machine.devices[":maincpu"].spaces["program"]
local function rd32(a) return mem:read_u32(a) end
local function rd8(a) return mem:read_u8(a) end
local function rds16(a) local v=mem:read_u16(a); if v>=0x8000 then v=v-0x10000 end; return v end
local f=io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/arch_deepfps.txt","w")
local function setbtn(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local fd=p.fields[n]; if fd then fd:set_value(v) end end end
local function release_all() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end end
local function census() local sched,total=0,0; for i=0,EMAX-1 do local b=ENTITIES+i*STRIDE; if rd8(b)~=0 then total=total+1; if rd8(b+OFF_FROMSCHED)~=0 then sched=sched+1 end end end; return sched,total end
local fr=0; local F_SEL=720; local F_DIP=930; local F_PLAY=1010
local subG,subF=nil,nil
local worstByRound={}     -- round -> worst 1s busy-scroll fps
local maxTotByRound={}
local prevCam=-1
local prevRnd,lastRndChange=1,0
emu.register_frame_done(function()
  fr=fr+1; release_all()
  if fr>=F_SEL and fr<F_SEL+8 then setbtn("P1 Start",1) end
  if fr>=F_DIP and fr<F_DIP+8 then setbtn("P1 Start",1) end
  if fr>=F_PLAY then
    -- PURE hold RIGHT + fire (NO lane weave) so the round-clear end-wall dwell
    -- accumulates and each round advances; this walks us up to the deep program.
    setbtn("P1 Right",1)
    if (fr%8)==0 then setbtn("P1 B",1) end
    local rnd=rd8(GGAME+ROUND_OFF); local cam=rds16(GGAME+CAM_OFF); local sched,total=census()
    if (maxTotByRound[rnd] or 0)<total then maxTotByRound[rnd]=total end
    if subG==nil then subG=rd32(FRAMECTR); subF=fr; prevCam=cam; prevRnd=rnd; lastRndChange=fr end
    if rnd~=prevRnd then lastRndChange=fr; prevRnd=rnd end
    if (fr%60)==0 then
      local g=rd32(FRAMECTR); local s=60.0*(g-subG)/(fr-subF)
      -- EXCLUDE the 1-frame round-transition DMA hitch (a full level VRAM reload in
      -- the unchanged game.c round_advance) -- it is the level-load cost, not the
      -- enemy-mix worst case. Only count steady dense+scrolling windows >=2s clear
      -- of a round change.
      if total>=8 and cam~=prevCam and (fr-lastRndChange)>120 then
        if worstByRound[rnd]==nil or s<worstByRound[rnd] then worstByRound[rnd]=s end
      end
      prevCam=cam; subG=g; subF=fr
    end
    if (fr%240)==0 then
      f:write(string.format("fr=%d round=%d cam=%d sched=%d total=%d\n", fr, rnd, cam, sched, total))
      f:flush()
    end
  end
  if fr>=4200 then
    f:write("\n=== WORST 1s busy+scrolling FPS per round reached (pure-right clears) ===\n")
    local rk={}; for k,_ in pairs(maxTotByRound) do rk[#rk+1]=k end; table.sort(rk)
    for _,r in ipairs(rk) do
      f:write(string.format("round %d : maxTotal=%d  worstBusyScrollFPS=%s\n",
        r, maxTotByRound[r], worstByRound[r] and string.format("%.2f",worstByRound[r]) or "n/a (no busy-scroll window)"))
    end
    f:close(); manager.machine:exit()
  end
end)
print("arch_deepfps running")
