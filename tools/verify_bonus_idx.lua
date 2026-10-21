-- Verify a SPECIFIC bonusLife index by poking g_config.bonusLife before play.
-- g_config @ 0xFF0004; fields dsw1,dsw2,lives,difficulty,bonusLife(@+4),... so
-- bonusLife @ 0xFF0008. Set via env SS_BL (0..3). Then poke score across the
-- thresholds for THAT index and confirm the exact award pattern.
local GCONFIG = 0xFF0004
local BL_OFF  = 4
local GGAME   = 0xFF0840
local SCORE_OFF, LIVES_OFF, BNEXT_OFF = 0x16, 0x1A, 0x1B
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local function rd32(a) return mem:read_u32(a) end
local function rd8(a) return mem:read_u8(a) end
local function wr32(a,v) mem:write_u32(a,v) end
local function wr8(a,v) mem:write_u8(a,v) end
local f = io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/verify_bonus_idx.txt","w")
local function log(s) f:write(s.."\n"); f:flush() end
local function setbtn(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local fd=p.fields[n]; if fd then fd:set_value(v) end end end
local function release_all() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end end

local BL = tonumber(os.getenv("SS_BL") or "3")
local fr=0; local F_SEL=720; local F_DIP=930; local F_PLAY=1010
local prevLives=nil; local prevScore=nil; local set_bl=false
-- thresholds per index (mirror game.c)
local TH={ [0]={30000,90000,120000}, [1]={20000,80000,100000}, [2]={30000}, [3]={20000} }
local pokes={}  -- frame -> score to poke (just under each threshold)
emu.register_frame_done(function()
  fr=fr+1; release_all()
  if fr>=F_SEL and fr<F_SEL+8 then setbtn("P1 Start",1) end
  if fr>=F_DIP and fr<F_DIP+8 then setbtn("P1 Start",1) end
  if fr>=F_PLAY then
    setbtn("P1 Right",1); if (fr%8)==0 then setbtn("P1 B",1) end
    local ph=(fr//40)%4; if ph==1 then setbtn("P1 Up",1) elseif ph==3 then setbtn("P1 Down",1) end
  end
  if fr==F_PLAY+5 and not set_bl then
    wr8(GCONFIG+BL_OFF, BL); set_bl=true
    -- also reset bonusNextIdx in case any threshold already passed at score 0
    wr8(GGAME+BNEXT_OFF, 0)
    log(string.format("[f%d] set g_config.bonusLife=%d (readback=%d). thresholds=%s",fr,BL,rd8(GCONFIG+BL_OFF), table.concat(TH[BL] or {}, "/")))
    -- schedule pokes: each at F_PLAY+200*k to (threshold-10), plus a final past-all poke
    local list=TH[BL] or {}
    for k,t in ipairs(list) do pokes[F_PLAY+300*k] = t-10 end
    pokes[F_PLAY+300*(#list+1)] = 250000   -- past all -> expect NO further award
    prevLives=rd8(GGAME+LIVES_OFF); prevScore=rd32(GGAME+SCORE_OFF)
  end
  if fr>F_PLAY then
    if pokes[fr] then wr32(GGAME+SCORE_OFF, pokes[fr]); prevScore=pokes[fr]
      log(string.format("[f%d] POKE score=%d (under next threshold)",fr,pokes[fr])) end
    local sc=rd32(GGAME+SCORE_OFF); local lv=rd8(GGAME+LIVES_OFF); local bn=rd8(GGAME+BNEXT_OFF)
    if lv~=prevLives then
      log(string.format("[f%5d] LIVES %d -> %d (score=%d bNext=%d) %s",fr,prevLives,lv,sc,bn, lv>prevLives and "<<< EXTRA LIFE" or "(death)"))
      prevLives=lv
    end
    prevScore=sc
  end
  if fr>=F_PLAY+300*5+150 then log("=== done (bonusLife="..BL..") ==="); manager.machine:exit() end
end)
