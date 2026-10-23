-- Read-only natural attract lifecycle. No RAM writes, CPU redirection or input.
local cpu=manager.machine.devices[":maincpu"]
local memory=cpu.spaces.program
local output=assert(io.open('out/parity-audit/attract-natural/trace.csv','w'))
output:write('frame,mode,state,substate,playstate,clock,timer,session,live,p1ptr,p2ptr,p1delay,p2delay,p1input,p2input,p1previous,p2previous,camera,p1active,p2active\n')
local frame=0
emu.register_frame_done(function()
    frame=frame+1
    local values={frame}
    for _,address in ipairs({2,5,8,11})do values[#values+1]=memory:read_u8(address)end
    values[#values+1]=memory:read_u16(0x23)
    values[#values+1]=memory:read_u16(3)
    values[#values+1]=memory:read_u8(0x99)
    values[#values+1]=memory:read_u8(0x22)
    values[#values+1]=memory:read_u16(0x4a)
    values[#values+1]=memory:read_u16(0x4c)
    for _,address in ipairs({0x48,0x49,0x44,0x45,0x46,0x47})do values[#values+1]=memory:read_u8(address)end
    values[#values+1]=memory:read_u16(0x90)
    values[#values+1]=memory:read_u8(0x480)
    values[#values+1]=memory:read_u8(0x4b0)
    output:write(table.concat(values,','),'\n')
    if frame==9000 then
        output:close()
        print('ATTRACT_NATURAL_DONE 9000')
        manager.machine:exit()
    end
end)
