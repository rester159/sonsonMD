-- verify_composite.lua : confirm the NATIVE port (genesis NTSC) renders gameplay
-- correctly at 60fps with the current single-16x16-per-entity approach (which the
-- RE proved matches the arcade -- no composite bodies exist on the playfield).
-- Front-end nav: pulse START while g_levelW==0 to reach the game, then run + fire,
-- snapshot a busy scene, and measure g_frameCounter / emu-frame.
-- THIS build (out/symbol.txt): g_frameCounter@0xFF007A g_game@0xFF07DC
--   g_levelW@0xFF0072 g_entities@0xFF0808 (Entity stride 0x2A).
local FRAMECTR = 0xFF007A
local LEVELW   = 0xFF0072
local ENTITIES = 0xFF0808
local STRIDE   = 0x2A
local EMAX     = 48
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local function rd32(a) return mem:read_u32(a) end
local function rd16(a) return mem:read_u16(a) end
local function rd8(a)  return mem:read_u8(a)  end
local function setbtn(name,v)
  local p=manager.machine.ioport.ports[":ctrl1:mdpad:PAD"]
  if p then local fd=p.fields[name]; if fd then fd:set_value(v) end end
end
local function release() for _,n in ipairs({"P1 Up","P1 Down","P1 Left","P1 Right","P1 Start","P1 A","P1 B","P1 C"}) do setbtn(n,0) end end

local fr=0
local m0,m1
local enemyMax=0
emu.register_frame_done(function()
  fr=fr+1; release()
  local lw = rd16(LEVELW)
  -- reach gameplay: pulse START whenever still in the front-end (levelW==0)
  if lw==0 then
    if (fr%30)<6 then setbtn("P1 Start",1) end
    return
  end
  -- in game: run right + fire, weave lanes
  setbtn("P1 Right",1)
  if (fr%7)<3 then setbtn("P1 B",1) end
  local ph=(fr//35)%4
  if ph==1 then setbtn("P1 Up",1) elseif ph==3 then setbtn("P1 Down",1) end

  -- count live enemy bodies (active@+0, type@+1)
  local n=0
  for i=0,EMAX-1 do local b=ENTITIES+i*STRIDE; if rd8(b)~=0 then n=n+1 end end
  if n>enemyMax then enemyMax=n end

  -- FPS window: lock m0 ~120 frames into gameplay, m1 +900 later; snapshot mid
  if not m0 and lw~=0 then m0frame=fr; m0=rd32(FRAMECTR) end
  if m0 and fr==(m0frame+450) then manager.machine.video:snapshot(); print("SNAP native_gameplay") end
  if m0 and fr==(m0frame+900) then
    m1=rd32(FRAMECTR)
    print(string.format("RESULT game-frames=%d over emu-frames=900 => FPS=%.3f ratio=%.4f",
      m1-m0, 60.0*(m1-m0)/900, (m1-m0)/900.0))
    print("max live entities seen = "..enemyMax)
    manager.machine:exit()
  end
  if fr>6000 then print("TIMEOUT levelW="..lw); manager.machine:exit() end
end)
print("verify_composite running")
