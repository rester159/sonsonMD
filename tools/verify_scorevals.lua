-- verify_scorevals.lua : confirm the corrected per-enemy SCORE_* in the NATIVE port
-- (mame genesis -cart out/rom.bin, USA NTSC 60Hz). Drives the front-end (mash Start), then
-- plays with the hero kept ALIVE (pin g_game.lives + flow=ALIVE + invuln) so we can stay in
-- play and rack up many kills. Logs each score delta tagged with the live entity census so
-- each delta maps to an EntityType, and measures FPS via g_frameCounter (constants-only change
-- must not move 60fps).
--
-- Symbols (out/symbol.txt 2026-06-17): g_frameCounter@0xFF007A, g_player_invuln@0xFF0078,
--   g_game@0xFF0818 (score u32 @+0x1A, lives u8 @+0x1E, flow u8 @+0x20), g_entities@0xFF0844
--   (stride 0x2A; active@+0, type@+1).
-- EntityType: 1 ENEMY_A 2 SHOT 3 ITEM 4 FLYER 5 THROWER 6 ENEMY_SHOT 7 CREATURE_G
--             8 CREATURE_C 9 COMPOSITE_E 10 HOPPER 11 LARGE 12 EMITTER 13 ORB 14 POPUP
local FRAMECTR=0xFF007A; local INVULN=0xFF0078
local GGAME=0xFF0818; local SCORE_OFF=0x1A; local LIVES_OFF=0x1E; local FLOW_OFF=0x20
local ENTITIES=0xFF0844; local EMAX,STRIDE,OFF_TYPE=48,0x2A,1
local TYPENAME={[1]="ENEMY_A",[2]="SHOT",[3]="ITEM",[4]="FLYER",[5]="THROWER",[6]="ESHOT",
  [7]="CREATURE_G",[8]="CREATURE_C",[9]="COMPOSITE_E",[10]="HOPPER",[11]="LARGE",[12]="EMITTER",[13]="ORB",[14]="POPUP"}
local mem=manager.machine.devices[":maincpu"].spaces["program"]
local function rd32(a) return mem:read_u32(a) end
local function rd8(a) return mem:read_u8(a) end
local function wr8(a,v) mem:write_u8(a,v) end
local f=io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/verify_scorevals.txt","w")
local function log(s) f:write(s.."\n"); f:flush() end
local function setbtn(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local fd=p.fields[n]; if fd then fd:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end end
local function census() local b2={}; local tot=0; for i=0,EMAX-1 do local b=ENTITIES+i*STRIDE; if rd8(b)~=0 then tot=tot+1; local t=rd8(b+OFF_TYPE); b2[t]=(b2[t] or 0)+1 end end; return b2,tot end

-- per-type tally: when the score jumps and exactly one enemy type's COUNT dropped vs last
-- frame, attribute the delta to that type (native analog of the arcade method). Also keep a
-- raw histogram and a "types present at delta" log.
local votes={}; local raw={}
local prevCount={}
local fr=0; local scoreLocked=false; local prevScore=nil; local play0=nil
local fpsM0=nil; local minSub=nil; local lastG=nil; local lastF=nil

emu.register_frame_done(function()
  fr=fr+1; rel()
  -- front-end: mash Start through selector/DIP until in play (score field starts moving / ents appear)
  if fr<1150 and (fr%24)<3 then setbtn("P1 Start",1) end
  -- detect in-play: entities present
  local byType,total=census()
  local inplay = total>0 and rd32(FRAMECTR)>0
  if inplay then
    if play0==nil then play0=fr; prevScore=rd32(GGAME+SCORE_OFF); log(string.format("[f%d] IN PLAY (score@+0x%X=%d)",fr,SCORE_OFF,prevScore)) end
    -- keep hero alive: pin lives, force flow ALIVE, invuln on
    wr8(GGAME+LIVES_OFF, 9)
    wr8(GGAME+FLOW_OFF, 0)
    wr8(INVULN, 1)
    -- movement: advance + fire + weave lanes; periodically reverse to re-engage spawns
    local seg=(fr//240)%4
    if seg==2 then setbtn("P1 Left",1) else setbtn("P1 Right",1) end
    if (fr%6)==0 then setbtn("P1 B",1) end
    local ph=(fr//30)%4; if ph==1 then setbtn("P1 Up",1) elseif ph==3 then setbtn("P1 Down",1) end

    local sc=rd32(GGAME+SCORE_OFF)
    if prevScore==nil then prevScore=sc end
    if sc~=prevScore and sc>prevScore then
      local d=sc-prevScore
      raw[d]=(raw[d] or 0)+1
      -- attribution by single enemy-type count drop
      local drop=nil; local nDrop=0
      local keys={}; for t in pairs(prevCount) do keys[t]=true end; for t in pairs(byType) do keys[t]=true end
      for t in pairs(keys) do
        if t~=2 and t~=3 and t~=6 and t~=14 then  -- ignore SHOT/ITEM/ESHOT/POPUP (not enemy kills)
          local now=byType[t] or 0; local was=prevCount[t] or 0
          if now<was then nDrop=nDrop+(was-now); if drop==nil then drop=t else drop=-1 end end
        end
      end
      local ts={}; for t,n in pairs(byType) do ts[#ts+1]=string.format("%s=%d",TYPENAME[t] or ("?"..t),n) end; table.sort(ts)
      local tag
      if drop and drop~=-1 and nDrop==1 then votes[drop]=votes[drop] or {}; votes[drop][d]=(votes[drop][d] or 0)+1; tag="DROP "..(TYPENAME[drop] or drop)
      else tag="(present:"..table.concat(ts," ")..")" end
      log(string.format("[f%5d] score +%-4d -> %d | %s", fr, d, sc, tag))
      prevScore=sc
    end
    prevCount=byType

    -- FPS
    if fpsM0==nil then fpsM0={fr, rd32(FRAMECTR)}; lastG=rd32(FRAMECTR); lastF=fr end
    if (fr%60)==0 and lastG then local g=rd32(FRAMECTR); if fr>lastF then local sub=60.0*(g-lastG)/(fr-lastF); if minSub==nil or sub<minSub then minSub=sub end end; lastG=g; lastF=fr end
  end

  if fr>=7000 then
    log("")
    log("=== PER-TYPE score deltas (single enemy-type count-drop attribution) ===")
    local ts={}; for t in pairs(votes) do ts[#ts+1]=t end; table.sort(ts)
    for _,t in ipairs(ts) do local ds={}; for d in pairs(votes[t]) do ds[#ds+1]=d end; table.sort(ds)
      local p={}; for _,d in ipairs(ds) do p[#p+1]=string.format("+%d x%d",d,votes[t][d]) end
      log(string.format("  %-12s : %s", TYPENAME[t] or t, table.concat(p,"  "))) end
    log("-- raw delta histogram --")
    local ds={}; for d in pairs(raw) do ds[#ds+1]=d end; table.sort(ds); local p={}; for _,d in ipairs(ds) do p[#p+1]=string.format("+%d x%d",d,raw[d]) end; log("  "..table.concat(p,"  "))
    if fpsM0 then local g=rd32(FRAMECTR); local fps=60.0*(g-fpsM0[2])/(fr-fpsM0[1]); log(string.format("=== FPS avg=%.2f worst60=%.2f over %d emu-frames ===",fps,minSub or -1,fr-fpsM0[1])) end
    f:close(); manager.machine:exit()
  end
end)
print("verify_scorevals running")
