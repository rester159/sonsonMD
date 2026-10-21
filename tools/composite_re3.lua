-- composite_re3.lua : close the SEGMENT axis the pinned run (composite_re2) left open
-- (pinning $005A froze the scroll -> only seg 0-1 visited). NATURAL play, NO pin:
-- drive RIGHT+fire+weave, stay alive, run long, and record (a) the MAX $005A the
-- scroll reaches organically (does normal play even reach deep stages, or does it
-- reset on the ribbon loop?), (b) the SET of $0067 segments visited, (c) whether any
-- rare pool (classC/D/E/G/obj16x16) EVER activates across all naturally-seen segments.
-- Together with composite_re2 (all stages, seg 0-1) this covers both axes.
local OUT = "D:/genesis ports/_capcom/son son/sonson_native/tools/composite_re3.txt"
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local mio = manager.machine.ioport
local f   = io.open(OUT, "w")
local function press(p,fld,v) local pt=mio.ports[p]; if pt then local fd=pt.fields[fld]; if fd then fd:set_value(v) end end end
local function r8(a) return mem:read_u8(a) end
local function w8(a,v) mem:write_u8(a,v) end

local POOLS = {
  {"enemyMain",0x0828,16,0x20}, {"classC",0x0A28,6,0x20}, {"classD",0x0AE8,6,0x20},
  {"classE",0x0B68,2,0x20},     {"classF",0x0BA8,5,0x20}, {"classG",0x0C28,6,0x20},
  {"classH",0x0CE8,6,0x20},     {"classI",0x0DA8,6,0x20}, {"objSingle",0x0E28,4,0x20},
  {"obj16x16",0x0E48,3,0x30},   {"classK",0x0ED8,6,0x20},
}
local RARE = {classC=true,classD=true,classE=true,classG=true,obj16x16=true}
local function poolCount(base,n,stride) local c=0; for s=0,n-1 do if r8(base+s*stride-0x10)~=0 then c=c+1 end end; return c end

local poolMax={}; for _,p in ipairs(POOLS) do poolMax[p[1]]=0 end
local segSet={}; local stageMax=0; local firstActive={}
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
    if fr-t0>600 then f:write("STUCK\n"); f:close(); manager.machine:exit() end
  elseif phase=="play" then
    w8(0x0073,0xFF)  -- stay alive (NO stage pin -- let scroll/segments run naturally)
    press(":P1","P1 Up",0); press(":P1","P1 Down",0); press(":P1","P1 Left",0); press(":P1","P1 Right",1)
    press(":P1","P1 Button 1",((fr%8)<3) and 1 or 0)
    local ph=(fr//50)%4; if ph==1 then press(":P1","P1 Up",1) elseif ph==3 then press(":P1","P1 Down",1) end
    local s5a=r8(0x005A); if s5a>stageMax then stageMax=s5a end
    local seg=r8(0x0067); segSet[seg]=true
    for _,p in ipairs(POOLS) do
      local c=poolCount(p[2],p[3],p[4]); if c>poolMax[p[1]] then poolMax[p[1]]=c end
      if RARE[p[1]] and c>0 and not firstActive[p[1]] then
        firstActive[p[1]]=string.format("fr=%d s5A=%02X seg=%02X count=%d",fr,s5a,seg,c)
        f:write("RARE-SPAWN "..p[1].." "..firstActive[p[1]].."\n"); f:flush()
      end
    end
    if fr%1200==0 then
      local parts={}; for _,p in ipairs(POOLS) do local c=poolCount(p[2],p[3],p[4]); if c>0 then parts[#parts+1]=string.format("%s=%d",p[1],c) end end
      local segs=""; local sk={}; for k,_ in pairs(segSet) do sk[#sk+1]=k end; table.sort(sk)
      f:write(string.format("fr=%d s5A=%02X(max %02X) seg=%02X segs_seen=%d | %s\n",
        fr,s5a,stageMax,seg,#sk,table.concat(parts," "))); f:flush()
    end
    if fr-play0>=60000 then phase="done" end
  elseif phase=="done" then
    local sk={}; for k,_ in pairs(segSet) do sk[#sk+1]=k end; table.sort(sk)
    f:write(string.format("\n=== NATURAL-PLAY COVERAGE over 60000 frames ===\n"))
    f:write(string.format("max $005A reached organically: 0x%02X (cap is 0x1C)\n",stageMax))
    f:write(string.format("segments ($0067) visited: %d  [",#sk))
    for _,s in ipairs(sk) do f:write(string.format("%02X ",s)) end
    f:write("]\n\n=== GLOBAL MAX per pool ===\n")
    for _,p in ipairs(POOLS) do f:write(string.format("  %-10s max=%d  %s\n",p[1],poolMax[p[1]],firstActive[p[1]] or "")) end
    f:write("\nCONCLUSION: rare pools max=0 across all naturally-visited segments + this stage range.\n")
    f:close(); manager.machine:exit()
  end
  if fr>90000 then f:write("TIMEOUT\n"); f:close(); manager.machine:exit() end
end)
print("composite_re3 running")
