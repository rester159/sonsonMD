-- arc_sprite_census.lua -- census EVERY (code,bank) the arcade renders during real
-- gameplay (mame sonson), to audit the native's baked sprite codes. Drive right +
-- survive; histogram all non-parked spriteram sprites with count + Y/X range.
-- Layout [sy,attr,code_lo,sx]; code=code_lo+((attr&0x20)<<3); bank=attr&0x1F.
local mem=manager.machine.devices[":maincpu"].spaces["program"]
local spr=manager.machine.memory.shares[":spriteram"]
local function r8(a) return mem:read_u8(a) end
local f=io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/arc_sprite_census.txt","w")
local function set(p,fl,v) local pp=manager.machine.ioport.ports[p]; if pp then local fd=pp.fields[fl]; if fd then fd:set_value(v) end end end
local function clr() for _,pf in ipairs({{":SYSTEM","Coin 1"},{":SYSTEM","1 Player Start"},
  {":P1","P1 Button 1"},{":P1","P1 Right"},{":P1","P1 Up"},{":P1","P1 Down"}}) do set(pf[1],pf[2],0) end end

local fr=0; local everInGame=false; local coinedAt=nil
local H={}                 -- "code/bank" -> {n,ymin,ymax,xmin,xmax}
emu.register_frame_done(function()
  fr=fr+1; clr()
  local gs=r8(0x0002)
  if gs==2 then everInGame=true end
  if gs~=2 then
    -- (re)enter a game whenever we're not in one (attract gs==1)
    if gs==1 then
      if coinedAt==nil then coinedAt=fr end
      local c=(fr-coinedAt)%140
      if c>=10 and c<35 then set(":SYSTEM","Coin 1",1)
      elseif c>=60 and c<90 then set(":SYSTEM","1 Player Start",1) end
    end
    return
  end
  coinedAt=nil
  mem:write_u8(0x0073,0xFF)
  set(":P1","P1 Right",1)
  if (fr%50)<3 then set(":P1","P1 Button 1",1) end
  local ph=(fr//40)%4
  if ph==1 then set(":P1","P1 Up",1) elseif ph==3 then set(":P1","P1 Down",1) end

  for s=0,23 do
    local o=s*4
    local sy=spr:read_u8(o); local attr=spr:read_u8(o+1); local cl=spr:read_u8(o+2); local sx=spr:read_u8(o+3)
    if not (sy==0xF8 and attr==0xF8) and sy~=0xF8 then
      local code=cl+((attr&0x20)<<3); local bank=attr&0x1F
      local k=string.format("code=%3d bank=%2d",code,bank)
      local e=H[k]; if not e then e={n=0,ymin=999,ymax=-1,xmin=999,xmax=-1}; H[k]=e end
      e.n=e.n+1
      if sy<e.ymin then e.ymin=sy end; if sy>e.ymax then e.ymax=sy end
      if sx<e.xmin then e.xmin=sx end; if sx>e.xmax then e.xmax=sx end
    end
  end

  if fr>=9000 then
    f:write("=== ARCADE SPRITE (code,bank) CENSUS over gameplay ===\n")
    local ks={}; for k,_ in pairs(H) do ks[#ks+1]=k end
    table.sort(ks, function(a,b) return H[a].n>H[b].n end)
    for _,k in ipairs(ks) do local e=H[k]
      f:write(string.format("  %s  n=%-6d y=[%d..%d] x=[%d..%d]\n",k,e.n,e.ymin,e.ymax,e.xmin,e.xmax))
    end
    f:close(); manager.machine:exit()
  end
end)
print("arc_sprite_census running")
