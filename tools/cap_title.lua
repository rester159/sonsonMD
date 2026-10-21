-- cap_title.lua -- dump the arcade TITLE-screen videoram/colorram/spriteram to JSON
-- (same format as scene_capture.json) so gen_assets can bake it natively.
local vram=manager.machine.memory.shares[":videoram"]
local cram=manager.machine.memory.shares[":colorram"]
local spr =manager.machine.memory.shares[":spriteram"]
local mem=manager.machine.devices[":maincpu"].spaces["program"]
local fr=0; local done=false
emu.register_frame_done(function()
  fr=fr+1
  if fr==1600 and not done then     -- title screen is up + stable (gs==1)
    done=true
    local f=io.open("D:/genesis ports/_capcom/son son/sonson_native/tools/title_capture.json","w")
    local function arr(share,n)
      local t={}
      for i=0,n-1 do t[#t+1]=tostring(share:read_u8(i)) end
      return table.concat(t,",")
    end
    f:write("{\n")
    f:write(string.format('  "frame": %d,\n  "md": 1,\n', fr))
    f:write('  "videoram": ['..arr(vram,1024)..'],\n')
    f:write('  "colorram": ['..arr(cram,1024)..'],\n')
    f:write('  "spriteram": ['..arr(spr,96)..']\n')
    f:write("}\n"); f:close()
    manager.machine.video:snapshot()
    print("title captured at fr="..fr)
    manager.machine:exit()
  end
end)
print("cap_title running")
