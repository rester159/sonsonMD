-- arc_align.lua : ARCADE ground truth for the player standing-Y per tier.
-- Coin+start into a credited game (md $0002: 01 attract -> 02 in-game), then read
-- the hero record Y + snapshot so we can see exactly where the arcade hero's feet
-- sit on each vine platform (and compare to the native pitch-32 k_spawn_row_y grid).
-- Hero record base $0490 (P1); Y byte at -$8 = $0488, X integer at -$B = $0485.
-- (RE per project memory + the port's player code.) Also dumps a few enemy slots.

local MD   = 0x0002
local HX   = 0x0485   -- hero worldX integer (-$B)
local HY   = 0x0488   -- hero Y byte (-$8)
local SCR  = 0x0090   -- coarse scroll

local mem
local function rd8(a) return mem:read_u8(a) end

local function setf(port, field, v)
  local p = manager.machine.ioport.ports[port]
  if p then local fd = p.fields[field]; if fd then fd:set_value(v); return true end end
  return false
end

local fr = 0
local enumerated = false
local coined, started = false, false
local snaps = 0
emu.register_frame_done(function()
  if not mem then mem = manager.machine.devices[":maincpu"].spaces["program"] end
  fr = fr + 1

  -- one-time: list ports/fields so we use the right names if the guesses miss.
  if not enumerated then
    enumerated = true
    for pn, p in pairs(manager.machine.ioport.ports) do
      local fs = {}
      for fn,_ in pairs(p.fields) do fs[#fs+1] = fn end
      print("PORT "..pn.." : "..table.concat(fs, " | "))
    end
  end

  local md = rd8(MD)
  -- release coin/start each frame (clean edges)
  setf(":SYSTEM","Coin 1",0); setf(":SYSTEM","1 Player Start",0)

  if fr == 300 then print(string.format("F%d md=%d (expect 1=attract)", fr, md)) end

  -- coin at 320, start at 380 (after the boot RAM-test ramp settles ~fr979? use later)
  if fr >= 1000 and fr < 1012 and not coined  then setf(":SYSTEM","Coin 1",1) end
  if fr == 1012 then coined = true end
  if fr >= 1080 and fr < 1092 and not started then setf(":SYSTEM","1 Player Start",1) end
  if fr == 1092 then started = true end

  if md == 2 then
    -- in-game: snapshot a few frames + log hero pos
    if (fr % 60) == 0 and snaps < 4 then
      print(string.format("INGAME F%d md=2 heroX=%d heroY=%d scroll=%d", fr, rd8(HX), rd8(HY), rd8(SCR)))
      manager.machine.video:snapshot(); snaps = snaps + 1
    end
    if snaps >= 4 then manager.machine:exit() end
  end
  if fr >= 2600 then print(string.format("END F%d md=%d", fr, md)); manager.machine:exit() end
end)
print("arc_align.lua running")
