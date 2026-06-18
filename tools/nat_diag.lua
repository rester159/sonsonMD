-- nat_diag.lua : find the front-end timing + score offset for THIS build. Press Start
-- repeatedly through the front-end, hold right+fire, and every 120 frames dump g_game[0..0x40]
-- as u32-at-even-offsets so we can SEE which word is the growing score. Also log g_frameCounter.
local GGAME=0xFF0818; local FRAMECTR=0xFF007A
local ENTITIES=0xFF0844; local EMAX,STRIDE,OFF_TYPE=48,0x2A,1
local mem=manager.machine.devices[":maincpu"].spaces["program"]
local function rd32(a) return mem:read_u32(a) end
local function rd8(a) return mem:read_u8(a) end
local f=io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/nat_diag.txt","w")
local function log(s) f:write(s.."\n"); f:flush() end
local function setbtn(n,v) local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]; if p then local fd=p.fields[n]; if fd then fd:set_value(v) end end end
local function rel() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end end
local function census() local b2={}; for i=0,EMAX-1 do local b=ENTITIES+i*STRIDE; if rd8(b)~=0 then local t=rd8(b+OFF_TYPE); b2[t]=(b2[t] or 0)+1 end end; local s=""; for t,n in pairs(b2) do s=s..t..":"..n.." " end; return s end
local fr=0
emu.register_frame_done(function()
  fr=fr+1; rel()
  -- mash Start every 30 frames through the whole front-end; after f1100 play
  if fr<1100 and (fr%30)<3 then setbtn("P1 Start",1) end
  if fr>=1100 then setbtn("P1 Right",1); if (fr%8)==0 then setbtn("P1 B",1) end
    local ph=(fr//40)%4; if ph==1 then setbtn("P1 Up",1) elseif ph==3 then setbtn("P1 Down",1) end end
  if (fr%120)==0 then
    local parts={}
    for off=0x10,0x24,2 do parts[#parts+1]=string.format("+%X=%d",off,rd32(GGAME+off)) end
    log(string.format("[f%5d] fc=%d ents{%s} | g_game: %s", fr, rd32(FRAMECTR), census(), table.concat(parts," ")))
  end
  if fr>=4000 then f:close(); manager.machine:exit() end
end)
print("nat_diag running")
