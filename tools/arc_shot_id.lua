-- arc_shot_id.lua -- resolve what pool $0E28 IS (player bullet vs orb) + capture its
-- live sprite code (mame sonson NTSC). Record: active@$0E18, code_lo@$0E28, attr@$0E29,
-- worldX@$0E1D(u16). Decode: full=code_lo+((attr&0x20)<<3); bank=attr&0x1F.
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local function r8(a) return mem:read_u8(a) end
local function r16(a) return mem:read_u16(a) end
local f = io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/arc_shot_id.txt","w")
local function set(port,field,v)
  local p=manager.machine.ioport.ports[port]; if p then local fd=p.fields[field]; if fd then fd:set_value(v) end end
end
local function clearall()
  for _,pf in ipairs({{":SYSTEM","Coin 1"},{":SYSTEM","1 Player Start"},{":P1","P1 Button 1"},
    {":P1","P1 Right"},{":P1","P1 Left"},{":P1","P1 Up"},{":P1","P1 Down"}}) do set(pf[1],pf[2],0) end
end

local fr=0
local seen = {}
local lastFire=-999
local activeNoFire=0
local logged=0
local maxgs=-1
local everInGame=false
local coinedAt=nil

emu.register_frame_done(function()
  fr=fr+1; clearall()
  local gs = r8(0x0002)
  if gs>maxgs then maxgs=gs end
  if gs==2 then everInGame=true end

  -- boot: wait for the attract state (gs==1, ~frame 1000), THEN coin + start (retry)
  if not everInGame then
    if gs==1 then
      if coinedAt==nil then coinedAt=fr end
      local t=fr-coinedAt
      local c=t%140
      if c>=10 and c<35 then set(":SYSTEM","Coin 1",1)
      elseif c>=60 and c<90 then set(":SYSTEM","1 Player Start",1) end
    end
  else
    mem:write_u8(0x0073, 0xFF)
    if (fr//30)%2==0 then set(":P1","P1 Right",1) end
    if (fr%45) < 3 then set(":P1","P1 Button 1",1); lastFire=fr end
  end

  if (fr%120)==0 then
    f:write(string.format("fr=%d gs($0002)=%d inGame=%s shotActive=%d\n",
      fr, gs, tostring(everInGame), r8(0x0E18))); f:flush()
  end

  if everInGame then
    local active = r8(0x0E18)
    if active ~= 0 then
      local codelo=r8(0x0E28); local attr=r8(0x0E29)
      local full=codelo+((attr&0x20)<<3); local bank=attr&0x1F
      local key=string.format("codelo=0x%02X attr=0x%02X -> code=%d bank=%d flipx=%d flipy=%d",
        codelo,attr,full,bank,(attr&0x40)==0 and 1 or 0,(attr&0x80)==0 and 1 or 0)
      seen[key]=(seen[key] or 0)+1
      if (fr-lastFire)>20 then activeNoFire=activeNoFire+1 end
      if logged<40 then
        f:write(string.format("  fr=%d %s wx=%d y=%d sinceFire=%d\n",fr,key,r16(0x0E1D),r8(0x0E20),fr-lastFire))
        logged=logged+1; f:flush()
      end
    end
  end

  if fr>=4500 then
    f:write(string.format("\nmax gs seen=%d  everInGame=%s\n", maxgs, tostring(everInGame)))
    f:write("=== DISTINCT (codelo,attr) SEEN ON $0E28 ===\n")
    local ks={}; for k,_ in pairs(seen) do ks[#ks+1]=k end; table.sort(ks)
    if #ks==0 then f:write("  (none -- pool never active)\n") end
    for _,k in ipairs(ks) do f:write(string.format("  %-70s x%d\n",k,seen[k])) end
    f:write(string.format("frames active with NO recent fire (>20f): %d\n", activeNoFire))
    f:close(); manager.machine:exit()
  end
end)
print("arc_shot_id running")
