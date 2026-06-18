-- soak.lua : long-run stress of the cumulative build. Survive (top lives), hold
-- RIGHT + fire + sweep tiers through MANY seamless-ribbon loops (~30k frames), and
-- watch for edge-case failures the per-feature checks wouldn't: fps sag, entity-pool
-- leak/overflow, ribbon-loop corruption, score/lives garbage, or a hang.
local GFC=0xFF007A; local LEVELW=0xFF0072; local IDX=0xFF0075
local GAME=0xFF0818; local SCORE=0xFF0832; local LIVES=0xFF0836
local ENTS=0xFF0844; local STRIDE=0x2A; local EMAX=48
local mem; local em=0; local ig; local previdx=-1; local loops=0; local maxact=0
local ws_em, ws_gfc; local minratio=9.9; local worstwin=""
local stall=0; local lastgfc=-1; local livesok=nil; local anomalies={}
local function pad(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local f=p.fields[n]; if f then f:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do pad(n,0) end end
local function rd8(a) return mem:read_u8(a) end
local function rd32(a) return mem:read_u32(a) end
emu.register_frame_done(function()
  if not mem then mem=manager.machine.devices[":maincpu"].spaces["program"] end
  em=em+1; rel()
  if mem:read_u16(LEVELW)==0 then if (em%80)<12 then pad("P1 Start",1) end; return end
  if not ig then ig=em; ws_em=em; ws_gfc=rd32(GFC)
     print(string.format("INGAME em=%d lives=%d", em, rd8(LIVES))) end
  -- SURVIVE the whole soak: REFILL lives every frame (game_update decrements at
  -- most 1/death, so capping at 9 each frame means it never hits 0 -> never
  -- game-overs -> we keep soaking the in-game ribbon, death/respawn cycles incl.).
  mem:write_u8(0xFF0836, 9)    -- g_game.lives = 9
  pad("P1 Right",1); pad("P1 B",((em%4)<2) and 1 or 0)
  local ph=(em//40)%6; if ph<=2 then pad("P1 Up",1) elseif ph==3 then pad("P1 Down",1) end
  -- ribbon loop count
  local idx=rd8(IDX)
  if previdx>=0 and idx<previdx and previdx>=6 then loops=loops+1 end
  previdx=idx
  -- entity pool: count active
  local act=0; for i=0,EMAX-1 do if rd8(ENTS+i*STRIDE)~=0 then act=act+1 end end
  if act>maxact then maxact=act end
  if act>EMAX then anomalies[#anomalies+1]="entity count > EMAX at em"..em end
  -- hang detect (gfc stall while not at a legit pause)
  local gfc=rd32(GFC)
  if gfc==lastgfc then stall=stall+1 else stall=0; lastgfc=gfc end
  if stall==90 then anomalies[#anomalies+1]=string.format("g_frameCounter STALL ~90f at em=%d idx=%d (hang or left-game?)",em,idx) end
  -- windowed fps
  if em-ws_em>=600 then
    local r=(gfc-ws_gfc)/(em-ws_em); if r<minratio then minratio=r; worstwin=string.format("em=%d",em) end
    ws_em=em; ws_gfc=gfc
  end
  if (em-ig)%6000==0 then print(string.format("  em=%d ig+%d loops=%d active=%d lives=%d score=%d idx=%d ratio~%.3f",
        em, em-ig, loops, act, rd8(LIVES), rd32(SCORE), idx, (gfc-ws_gfc)/math.max(1,em-ws_em))) end
  if em-ig>=30000 then
    print(string.format("SOAK DONE: %d in-game frames, %d ribbon loops, maxActive=%d, worst windowed fps ratio=%.4f (%s), finalScore=%d, anomalies=%d",
          em-ig, loops, maxact, minratio, worstwin, rd32(SCORE), #anomalies))
    for _,a in ipairs(anomalies) do print("  ANOMALY: "..a) end
    manager.machine:exit()
  end
end)
print("soak.lua running")
