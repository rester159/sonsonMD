-- arc_bullet.lua -- find the PLAYER BULLET sprite empirically from hardware spriteram.
-- Stand still; fire single shots on a cadence; the sprite that consistently APPEARS
-- right after a fire (and isn't in the standing baseline) is the bullet. Dump raw
-- 4-byte slots so the layout is unambiguous. (mame sonson NTSC.)
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local spr = manager.machine.memory.shares[":spriteram"]
local function r8(a) return mem:read_u8(a) end
local f = io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/arc_bullet.txt","w")
local function set(port,field,v)
  local p=manager.machine.ioport.ports[port]; if p then local fd=p.fields[field]; if fd then fd:set_value(v) end end
end
local function clearall()
  for _,pf in ipairs({{":SYSTEM","Coin 1"},{":SYSTEM","1 Player Start"},{":P1","P1 Button 1"},
    {":P1","P1 Right"},{":P1","P1 Left"}}) do set(pf[1],pf[2],0) end
end

local fr=0
local everInGame=false
local coinedAt=nil
local NSPR=24
-- baseline set of (b2,b3) [candidate code,attr] present while standing & NOT firing
local postFireNew = {}   -- "b2/b3" -> count  (sprites appearing shortly after a fire)
local baseline = {}      -- "b2/b3" -> seen while idle (no recent fire)
local lastFire=-999
local dumped=0

local function slots()
  local t={}
  for s=0,NSPR-1 do
    local o=s*4
    t[s]={spr:read_u8(o),spr:read_u8(o+1),spr:read_u8(o+2),spr:read_u8(o+3)}
  end
  return t
end

emu.register_frame_done(function()
  fr=fr+1; clearall()
  local gs=r8(0x0002)
  if gs==2 then everInGame=true end
  if not everInGame then
    if gs==1 then
      if coinedAt==nil then coinedAt=fr end
      local c=(fr-coinedAt)%140
      if c>=10 and c<35 then set(":SYSTEM","Coin 1",1)
      elseif c>=60 and c<90 then set(":SYSTEM","1 Player Start",1) end
    end
    return
  end
  mem:write_u8(0x0073,0xFF)                 -- keep-alive (lives)
  -- STAND STILL. Fire a single shot every 90 frames.
  local firing = (fr%90)==10
  if firing then set(":P1","P1 Button 1",1); lastFire=fr end

  local sinceFire = fr-lastFire
  local sl=slots()
  for s=0,NSPR-1 do
    local b=sl[s]                                  -- [sy, attr, code_lo, sx]
    local sy,attr,code_lo,sx = b[1],b[2],b[3],b[4]
    local key=string.format("attr=0x%02X code_lo=0x%02X",attr,code_lo)
    if sinceFire>50 and sinceFire<85 then
      baseline[key]=true                           -- idle baseline (no shot)
    elseif sinceFire>=1 and sinceFire<=30 then
      local e=postFireNew[key]
      if not e then e={n=0,xmin=999,xmax=-1,sy=sy}; postFireNew[key]=e end
      e.n=e.n+1; if sx<e.xmin then e.xmin=sx end; if sx>e.xmax then e.xmax=sx end
    end
  end

  -- raw dump of all non-zero slots for a few frames right after the FIRST few fires
  if sinceFire>=1 and sinceFire<=6 and dumped<60 then
    f:write(string.format("fr=%d sinceFire=%d :\n", fr, sinceFire))
    for s=0,NSPR-1 do
      local b=sl[s]
      if not (b[1]==0 and b[2]==0 and b[3]==0) then
        f:write(string.format("  slot %2d: %02X %02X %02X %02X\n", s,b[1],b[2],b[3],b[4]))
      end
    end
    dumped=dumped+1; f:flush()
  end

  if fr>=4000 then
    f:write("\n=== sprites 1-30f AFTER fire, NOT in idle baseline (key=attr,code_lo) ===\n")
    f:write("    full_code=code_lo+((attr&0x20)<<3); bank=attr&0x1F; flipx=(attr&0x40)?0:1\n")
    local ks={}; for k,_ in pairs(postFireNew) do if not baseline[k] then ks[#ks+1]=k end end
    table.sort(ks, function(a,b) return postFireNew[a].n>postFireNew[b].n end)
    for _,k in ipairs(ks) do
      local e=postFireNew[k]
      local attr=tonumber(k:match("attr=0x(%x%x)"),16)
      local cl=tonumber(k:match("code_lo=0x(%x%x)"),16)
      local full=cl+((attr&0x20)<<3); local bank=attr&0x1F
      f:write(string.format("  %s -> code=%d bank=%d flipx=%d | n=%d sx=[%d..%d] dx=%d sy=%d\n",
        k, full, bank, (attr&0x40)==0 and 1 or 0, e.n, e.xmin, e.xmax, e.xmax-e.xmin, e.sy))
    end
    f:write("\n(bullet = wide dx (fast horizontal travel); pose = dx~0 at player X)\n")
    f:close(); manager.machine:exit()
  end
end)
print("arc_bullet running")
