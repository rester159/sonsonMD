-- arc_regions.lua -- list memory regions + shares (find spriteram) for sonson
local f = io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/arc_regions.txt","w")
local fr=0
emu.register_frame_done(function()
  fr=fr+1
  if fr==2 then
    f:write("=== REGIONS ===\n")
    for name,r in pairs(manager.machine.memory.regions) do
      f:write(string.format("  %-24s size=%d\n", name, r.size))
    end
    f:write("=== SHARES ===\n")
    for name,s in pairs(manager.machine.memory.shares) do
      f:write(string.format("  %-24s size=%d\n", name, s.size))
    end
    f:flush(); f:close(); manager.machine:exit()
  end
end)
print("arc_regions running")
