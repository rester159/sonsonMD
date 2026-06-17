-- Verify the native port's SCORING + EXTRA-LIFE (mame genesis -cart out/rom.bin).
-- Drives the front-end like arch_verify (P1 Start at the selector + DIP), then plays.
-- Reads g_game: score u32 @+0x16 (stable), lives u8 @+0x1A, bonusNextIdx u8 @+0x1B,
-- round u8 @+0x20 (recomputed after adding bonusNextIdx). Logs:
--   (A) per-frame score deltas during play (the RE'd point awards land here);
--   (B) a forced EXTRA-LIFE test: once in-game, POKE score to 19990 and confirm the
--       next award crosses 20000 -> lives++ (default bonusLife idx1 = 20K/80K/100K).
local GGAME    = 0xFF0840
local SCORE_OFF, LIVES_OFF, BNEXT_OFF, ROUND_OFF = 0x16, 0x1A, 0x1B, 0x20
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local function rd32(a) return mem:read_u32(a) end
local function rd8(a)  return mem:read_u8(a) end
local function wr32(a,v) mem:write_u32(a,v) end
local f = io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/verify_scoring.txt","w")
local function log(s) f:write(s.."\n"); f:flush() end

local function setbtn(name,v)
  local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]
  if p then local fd=p.fields[name]; if fd then fd:set_value(v) end end
end
local function release_all()
  for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end
end

local fr=0
local F_SEL=720; local F_DIP=930; local F_PLAY=1010
local prevScore=nil; local prevLives=nil
local poked=false; local pokeFrame=nil
emu.register_frame_done(function()
  fr=fr+1; release_all()
  if fr>=F_SEL and fr<F_SEL+8 then setbtn("P1 Start",1) end
  if fr>=F_DIP and fr<F_DIP+8 then setbtn("P1 Start",1) end
  if fr>=F_PLAY then
    setbtn("P1 Right",1)
    if (fr%8)==0 then setbtn("P1 B",1) end
    local ph=(fr//40)%4
    if ph==1 then setbtn("P1 Up",1) elseif ph==3 then setbtn("P1 Down",1) end
  end

  if fr>=F_PLAY then
    local sc=rd32(GGAME+SCORE_OFF)
    local lv=rd8(GGAME+LIVES_OFF)
    local bn=rd8(GGAME+BNEXT_OFF)
    if prevScore==nil then prevScore=sc; prevLives=lv; log(string.format("[f%d] PLAY start score=%d lives=%d bonusNextIdx=%d round=%d",fr,sc,lv,bn,rd8(GGAME+ROUND_OFF))) end
    if sc~=prevScore then
      log(string.format("[f%5d] score %d -> %d  (+%d)  lives=%d bNext=%d", fr, prevScore, sc, sc-prevScore, lv, bn))
      prevScore=sc
    end
    if lv~=prevLives then
      log(string.format("[f%5d] *** LIVES %d -> %d  (score=%d bNext=%d)  <<< EXTRA LIFE",fr,prevLives,lv,sc,bn))
      prevLives=lv
    end
    -- forced extra-life test: at f1600, poke score to 19990 so the next award (the
    -- RIGHT-running player keeps killing) crosses the 20000 default threshold.
    if fr==1600 and not poked then
      wr32(GGAME+SCORE_OFF, 19990); poked=true; pokeFrame=fr; prevScore=19990
      log(string.format("[f%d] POKED score=19990 (just under default 20K bonus); expect lives++ on next award",fr))
    end
    -- second forced test: at f2200 poke to 79990 to cross the 80K (second idx1 threshold)
    if fr==2200 then wr32(GGAME+SCORE_OFF, 79990); prevScore=79990
      log(string.format("[f%d] POKED score=79990 (just under 80K); expect lives++ on next award",fr)) end
    -- third: at f2800 poke to 99990 to cross 100K (third + final idx1 threshold)
    if fr==2800 then wr32(GGAME+SCORE_OFF, 99990); prevScore=99990
      log(string.format("[f%d] POKED score=99990 (just under 100K); expect lives++ ; after this NO more (list end)",fr)) end
    -- fourth: poke to 199990 (well past all) to confirm NO further award (idx1 only 3)
    if fr==3400 then wr32(GGAME+SCORE_OFF, 199990); prevScore=199990
      log(string.format("[f%d] POKED score=199990 (past all idx1 thresholds); expect NO further lives++",fr)) end
  end

  if fr>=4200 then log("=== done ==="); manager.machine:exit() end
end)
