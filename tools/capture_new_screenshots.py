#!/usr/bin/env python3
"""Capture ten fresh, controller-navigated UI/gameplay screenshots, headlessly."""
from pathlib import Path
import subprocess,hashlib,json,datetime
r=Path(__file__).resolve().parents[1]
o=r/'out'/('screenshots-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'));o.mkdir(parents=True)
scenes=[
 ('arcade',[(950,965,1,'Start'),(1140,1155,1,'Start'),(1300,1312,1,'C'),(1350,1362,1,'Start'),(1540,1552,2,'C'),(1590,1602,2,'Start')],[(900,'01-opening-menu'),(1050,'02-arcade-dip-switches'),(1240,'03-arcade-title'),(1500,'04-arcade-one-player'),(1770,'05-arcade-two-players')],1800),
 ('home',[(920,932,1,'Down'),(950,965,1,'Start'),(1040,1052,1,'Up'),(1120,1132,1,'Start'),(1450,1462,1,'Start'),(1540,1552,1,'Down'),(1570,1582,1,'Down'),(1630,1642,1,'Start')],[(1020,'06-home-menu'),(1360,'07-home-options'),(1820,'08-home-two-players')],1850),
 ('survival-1p',[(900,912,1,'Down'),(940,952,1,'Start'),(1040,1052,1,'Down'),(1080,1092,1,'Down'),(1160,1172,1,'Start')],[(1360,'09-instadeath-one-player')],1390),
 ('survival-2p',[(900,912,1,'Down'),(940,952,1,'Start'),(1040,1052,1,'Down'),(1080,1092,1,'Down'),(1120,1132,1,'Down'),(1160,1172,1,'Start')],[(1360,'10-instadeath-two-players')],1390)]
manifest=[]
for name,inputs,shots,end in scenes:
 d=o/name;d.mkdir()
 lua='''local fr=0
emu.register_frame_done(function()
 fr=fr+1
 local function pad(k,n,v) manager.machine.ioport.ports[":ctrl"..k..":mdpad:PAD"].fields["P"..k.." "..n]:set_value(v) end
 for k=1,2 do for _,n in ipairs({"Down","Up","Start","C","B"}) do pad(k,n,0) end end
'''
 for a,b,k,button in inputs:lua+=f' if fr>={a} and fr<{b} then pad({k},"{button}",1) end\n'
 # Shoot during gameplay using ordinary controller input.
 lua+=f' if fr>{1690 if name=="home" else 1200} and fr%20<5 then pad(1,"B",1);pad(2,"B",1) end\n'
 for fr,label in shots:lua+=f' if fr=={fr} then manager.machine.video:snapshot() end\n'
 lua+=f' if fr>={end} then manager.machine:exit() end\nend)\n'
 (d/'run.lua').write_text(lua)
 cmd=['mame','genesis','-cart',str(r/'out/rom.bin'),'-autoboot_script',str(d/'run.lua'),'-window','-video','none','-keyboardprovider','none','-mouseprovider','none','-joystickprovider','none','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-nvram_directory',str(d/'nvram'),'-snapshot_directory',str(d/'raw')]
 subprocess.run(cmd,cwd=r,stdout=(d/'mame.txt').open('w'),stderr=subprocess.STDOUT,check=True)
 images=sorted((d/'raw').rglob('*.png'));assert len(images)==len(shots),(name,len(images))
 for image,(frame,label) in zip(images,shots):
  target=o/(label+'.png');target.write_bytes(image.read_bytes());manifest.append({'file':target.name,'frame':frame,'scenario':name})
 print(name,'captured',len(images),flush=True)
(o/'manifest.json').write_text(json.dumps({'rom_sha256':hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest(),'screenshots':manifest},indent=2)+'\n')
print(o,flush=True)
