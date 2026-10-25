from PIL import Image,ImageDraw
from pathlib import Path
import zipfile,hashlib,json
p=Path.cwd()/'itch-screenshots';w=p/'work';upload=p/'upload';upload.mkdir(exist_ok=True)
choices=[('game',2,'01-coop-grasslands'),('game',5,'02-blue-platforms'),('game',12,'03-purple-platforms'),('game',16,'04-stone-ruins'),('game',23,'05-brick-transition'),('game',27,'06-brickwork-battle'),('game',31,'07-blue-stone'),('game',36,'08-late-stage-coop'),('menus',0,'09-title-screen'),('menus',1,'10-home-options')]
contact=Image.new('RGB',(768*2,712*5),'#202020');d=ImageDraw.Draw(contact)
manifest=[]
for i,(group,n,name) in enumerate(choices):
 src=w/group/'genesis'/f'{n:04}.png';im=Image.open(src).convert('RGB');big=im.resize((im.width*3,im.height*3),Image.Resampling.NEAREST);target=upload/(name+'.png');big.save(target)
 x=i%2*768;y=i//2*712;contact.paste(big,(x,y));d.text((x+8,y+678),name,fill='white');manifest.append({'file':target.name,'source':str(src),'native_size':im.size})
contact.save(p/'contact-sheet.jpg',quality=95)
with zipfile.ZipFile(p/'sonson-itch-screenshots.zip','w',zipfile.ZIP_DEFLATED) as z:
 for f in sorted(upload.glob('*.png')):z.write(f,f.name)
(w/'capture-manifest.json').write_text(json.dumps({'rom_sha256':hashlib.sha256((p.parent/'sonson_native/out/rom.bin').read_bytes()).hexdigest(),'note':'MAME captures; nearest-neighbor 3x enlargement. Automated co-op capture replenishes lives to 3 for scenery coverage. In-game development HUD retained.','screenshots':manifest},indent=2))
print('Created 10 PNGs, ZIP and contact sheet')
