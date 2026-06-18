-- composite_re2.lua : DEFINITIVE deep-stage spawn census for the "rare archetype"
-- question the prior run (composite_re, reached only stage 0x0B) left open.
--
-- The prior run bumped $005A +1/900f, but $005A is SCROLL-DRIVEN and resets on the
-- ribbon loop, so it never passed ~0x0B. $0096 (the wave selector) is DERIVED from
-- $005A so poking it alone is overwritten. FIX: FORCE-PIN $005A (and $0096) every
-- frame to a target that RAMPS 0 -> 0x1C (the true cap), HOLDING each stage ~1500
-- frames -- this both (a) crosses every stage boundary (spawn-on-transition) and
-- (b) dwells at each (spawn-on-hold) and (c) actually REACHES 0x1C. Meanwhile drive
-- RIGHT so the gfx-segment $0067 sweeps all 26 segments (segment-gated placement).
--
-- QUESTION: do classC($0A28) classD($0AE8) classE($0B68=COMPOSITE_E) classG($0C28)
-- obj16x16($0E48=LARGE) EVER go active in real md=02 play?  If NO even pinned at the
-- max stage across all segments -> the native roster spawning CREATURE_C/CREATURE_G/
-- EMITTER/LARGE/ORB/COMPOSITE_E is UNFAITHFUL and must be gated.  If YES -> log the
-- stage/segment/score so we learn their real context + can measure their scores.
local OUT = "D:/genesis ports/_capcom/son son/sonson_native/tools/composite_re2.txt"
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local mio = manager.machine.ioport
local f   = io.open(OUT, "w")
local function press(p, fld, v) local pt=mio.ports[p]; if pt then local fd=pt.fields[fld]; if fd then fd:set_value(v) end end end
local function r8(a) return mem:read_u8(a) end
local function w8(a,v) mem:write_u8(a,v) end
local function score()  -- $006C-6F BCD, displayed 1:1
  return string.format("%02X%02X%02X%02X", r8(0x006C), r8(0x006D), r8(0x006E), r8(0x006F))
end

local POOLS = {
  {"enemyMain",0x0828,16,0x20}, {"classC",0x0A28,6,0x20}, {"classD",0x0AE8,6,0x20},
  {"classE",0x0B68,2,0x20},     {"classF",0x0BA8,5,0x20}, {"classG",0x0C28,6,0x20},
  {"classH",0x0CE8,6,0x20},     {"classI",0x0DA8,6,0x20}, {"objSingle",0x0E28,4,0x20},
  {"obj16x16",0x0E48,3,0x30},   {"classK",0x0ED8,6,0x20},
}
local RARE = {classC=true,classD=true,classE=true,classG=true,obj16x16=true}
local function poolCount(base,n,stride)
  local c=0; for s=0,n-1 do if r8(base+s*stride-0x10)~=0 then c=c+1 end end; return c
end

local poolMax={}; for _,p in ipairs(POOLS) do poolMax[p[1]]=0 end
local stageSeen={}; local firstActive={}; local segSeen={}
local fr,phase,t0,play0 = 0,"attract",0,nil

emu.register_frame_done(function()
  fr=fr+1
  local md=r8(0x0002)
  if phase=="attract" then
    if md==0x01 and fr>700 then phase="coin"; t0=fr end
  elseif phase=="coin" then
    press(":SYSTEM","Coin 1",((fr-t0)%24<6) and 1 or 0)
    if fr-t0>=96 then phase="start"; t0=fr; press(":SYSTEM","Coin 1",0) end
  elseif phase=="start" then
    press(":SYSTEM","1 Player Start",((fr-t0)%24<6) and 1 or 0)
    if md==0x02 then phase="play"; t0=fr; play0=fr; press(":SYSTEM","1 Player Start",0) end
    if fr-t0>600 then f:write("STUCK at start\n"); f:close(); manager.machine:exit() end
  elseif phase=="play" then
    w8(0x0073,0xFF)  -- stay alive
    press(":P1","P1 Up",0); press(":P1","P1 Down",0); press(":P1","P1 Left",0); press(":P1","P1 Right",1)
    press(":P1","P1 Button 1",((fr%8)<3) and 1 or 0)
    local ph=(fr//50)%4; if ph==1 then press(":P1","P1 Up",1) elseif ph==3 then press(":P1","P1 Down",1) end
    -- FORCE-PIN the stage to a ramping target (hold each ~1500f, reach 0x1C)
    local elapsed=fr-play0
    local target=elapsed//1500; if target>0x1C then target=0x1C end
    w8(0x005A,target); w8(0x0096,target)
    local stage=target
    local seg=r8(0x0067)
    segSeen[seg]=true
    stageSeen[stage]=stageSeen[stage] or {}
    for _,p in ipairs(POOLS) do
      local c=poolCount(p[2],p[3],p[4])
      if c>(stageSeen[stage][p[1]] or 0) then stageSeen[stage][p[1]]=c end
      if c>poolMax[p[1]] then poolMax[p[1]]=c end
      if RARE[p[1]] and c>0 and not firstActive[p[1]] then
        firstActive[p[1]]=string.format("fr=%d stage=%02X seg=%02X count=%d score=%s",fr,stage,seg,c,score())
        f:write("RARE-SPAWN "..p[1].." "..firstActive[p[1]].."\n"); f:flush()
      end
    end
    if fr%600==0 then
      local parts={}; for _,p in ipairs(POOLS) do local c=poolCount(p[2],p[3],p[4]); if c>0 then parts[#parts+1]=string.format("%s=%d",p[1],c) end end
      f:write(string.format("fr=%d stage=%02X seg=%02X score=%s | %s\n",fr,stage,seg,score(),table.concat(parts," "))); f:flush()
    end
    if elapsed>=45000 then phase="done" end
  elseif phase=="done" then
    f:write("\n=== PER-STAGE rare-pool max (blank = none of classC/D/E/G/obj16x16 ever active) ===\n")
    local sk={}; for k,_ in pairs(stageSeen) do sk[#sk+1]=k end; table.sort(sk)
    for _,st in ipairs(sk) do
      local parts={}; for _,p in ipairs(POOLS) do if RARE[p[1]] then local m=stageSeen[st][p[1]]; if m and m>0 then parts[#parts+1]=string.format("%s=%d",p[1],m) end end end
      f:write(string.format("stage %02X : %s\n",st,(#parts>0) and table.concat(parts,"  ") or "(no rare)"))
    end
    local nseg=0; for _,_ in pairs(segSeen) do nseg=nseg+1 end
    f:write(string.format("\nsegments visited: %d\n",nseg))
    f:write("\n=== GLOBAL MAX per pool ===\n")
    for _,p in ipairs(POOLS) do f:write(string.format("  %-10s max=%d  %s\n",p[1],poolMax[p[1]],firstActive[p[1]] or "")) end
    f:write("\nCONCLUSION: rare pools with max=0 NEVER spawn even pinned at stage 0x1C across all segments.\n")
    f:close(); manager.machine:exit()
  end
  if fr>90000 then f:write("TIMEOUT\n"); f:close(); manager.machine:exit() end
end)
print("composite_re2 running")
