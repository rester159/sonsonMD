#!/usr/bin/env python3
"""Complete large actor oracle on a RAM-loaded native 68000 component."""
import hashlib,json,re,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/large-actor-target';o.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
trace=r/'out/parity-audit/large-motion-oracle.csv';report=json.loads((r/'out/parity-audit/large-actor-comparison.json').read_text())
assert sha(trace)==report['trace_sha256']
for name,digest in report['source_sha256'].items():assert sha(r/'src'/name)==digest
report['native_sha256']=report['source_sha256']['arcade_large_actor.c']
cases=r/'out/parity-audit/large-actor-target.csv';assert sha(cases)==report['target_sha256'];(o/'cases.csv').write_bytes(cases.read_bytes())
(o/'wrapper.c').write_text('#include "arcade_large.h"\nvoid test_large(const u8 *v,u8 *out) {\n ArcadeLargeActor a;ArcadeLargeBody *b=&a.body;ArcadeFoeWorld w;\n ArcadeLargeShotBirth birth={0x1234,0x56,0x78,0x9a};\n u8 k,sample=v[50];u16 rng=(u16)((v[48]<<8)|v[49]);\n b->animation.cursor=(u16)((v[0]<<8)|v[1]);b->animation.timer=v[2];\n for(k=0;k<4;k++){b->animation.code[k]=v[3+k];b->animation.attr[k]=v[7+k];}\n b->active=v[11];b->state=v[12];b->phase=v[13];b->subtype=v[14];b->variant=v[15];b->owner=v[16];b->hp=v[17];b->timer=v[18];\n a.x=(u16)((v[19]<<8)|v[20]);a.xf=v[21];a.y=v[22];a.yf=v[23];a.direction=v[24];a.facing=v[25];a.hit=v[26];a.lifetime=v[27];a.fire_timer=v[28];a.fire_period=v[29];a.target=v[30];a.retreat=v[31];\n w.stage=w.shot_budget=w.purge=0;w.camera=(u16)((v[32]<<8)|v[33]);w.clock=v[34];w.tracking=v[35];w.scroll_stopped=v[36];\n for(k=0;k<2;k++){const u8 *p=v+38+k*5;w.players[k].x=(u16)((p[0]<<8)|p[1]);w.players[k].y=p[2];w.players[k].active=p[3];w.players[k].kind=p[4];}\n out[32]=arcade_large_step(&a,&w,v[37],&rng,&sample,v+51,&birth);\n out[0]=(u8)(b->animation.cursor>>8);out[1]=(u8)b->animation.cursor;out[2]=b->animation.timer;\n for(k=0;k<4;k++){out[3+k]=b->animation.code[k];out[7+k]=b->animation.attr[k];}\n out[11]=b->active;out[12]=b->state;out[13]=b->phase;out[14]=b->subtype;out[15]=b->variant;out[16]=b->owner;out[17]=b->hp;out[18]=b->timer;\n out[19]=(u8)(a.x>>8);out[20]=(u8)a.x;out[21]=a.xf;out[22]=a.y;out[23]=a.yf;out[24]=a.direction;out[25]=a.facing;out[26]=a.hit;out[27]=a.lifetime;out[28]=a.fire_timer;out[29]=a.fire_period;out[30]=a.target;out[31]=a.retreat;\n out[33]=(u8)(birth.x>>8);out[34]=(u8)birth.x;out[35]=birth.y;out[36]=birth.facing;out[37]=birth.slot;\n out[38]=(u8)(rng>>8);out[39]=(u8)rng;out[40]=sample;\n}\n')
sdk=Path('/Users/rester159/My_apps/portForge/tools/sgdk');cc='/opt/homebrew/bin/m68k-elf-gcc'
flags=['-m68000','-DSGDK_GCC','-O2','-fno-builtin','-fomit-frame-pointer','-fms-extensions','-I'+str(r/'inc'),'-I'+str(sdk/'inc'),'-I'+str(sdk/'res')]
objects=[]
for source in [r/'src'/name for name in report['source_sha256']]+[o/'wrapper.c']:
 obj=o/(source.stem+'.o');subprocess.run([cc,*flags,'-c',str(source),'-o',str(obj)],check=True);objects.append(str(obj))
subprocess.run(['m68k-elf-ld','-Ttext=0xff9000','--entry=test_large',*objects,'-o',str(o/'code.elf')],check=True)
subprocess.run(['m68k-elf-objcopy','-O','binary',str(o/'code.elf'),str(o/'code.bin')],check=True)
nm=subprocess.check_output(['m68k-elf-nm',str(o/'code.elf')],text=True);entry=int(re.search(r'^([0-9a-f]+) T test_large$',nm,re.M)[1],16);assert (o/'code.bin').stat().st_size<8192
s='''local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local input=assert(io.open(INPUT,'r'));local code=assert(io.open(CODE,'rb'));local binary=code:read('*a');code:close()
local frame=0;local pending=nil;local terminal=false;local count=0
local function finish(err)terminal=true;input:close();local f=assert(io.open(RESULT,'w'));f:write(string.format('{"cases":%d,"error":%s}',count,err and string.format('%q',err)or'null'));f:close();manager.machine:exit()end
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state['CURPC'].value&0xffffff==0xffb046,'return')
   for k=0,40 do assert(m:read_u8(0xffb300+k)==tonumber(pending:sub(k*2+1,k*2+2),16),'case '..count..' output '..k)end
   count=count+1
   if count%4096==0 then local p=assert(io.open(PROGRESS,'w'));p:write(tostring(count));p:close()end
  else for k=1,#binary do m:write_u8(0xff9000+k-1,binary:byte(k))end end
  local line=input:read('*l');if not line then finish();return end
  local src,expected=line:match('([^,]+),([^,]+)');pending=expected
  for k=0,#src//2-1 do m:write_u8(0xffb200+k,tonumber(src:sub(k*2+1,k*2+2),16))end
  for k=0,40 do m:write_u8(0xffb300+k,0xa5)end
  for a=0xffb000,0xffb03e,2 do m:write_u16(a,0x4e71)end
  m:write_u16(0xffb040,0x4eb9);m:write_u32(0xffb042,ENTRY);m:write_u16(0xffb046,0x60fe)
  m:write_u32(0xffbfe0,0xffb200);m:write_u32(0xffbfe4,0xffb300)
  cpu.state['SR'].value=0x2700;cpu.state['SP'].value=0xffbfe0;cpu.state['PC'].value=0xffb000
 end)
 if not ok then finish(err)end
end)
'''
for k,v in {'PROGRESS':str(o/'progress.txt'),'INPUT':str(o/'cases.csv'),'CODE':str(o/'code.bin'),'RESULT':str(o/'results.json')}.items():s=re.sub(r'\b'+k+r'\b',json.dumps(v),s)
s=s.replace('ENTRY',str(entry));(o/'run.lua').write_text(s);(o/'results.json').unlink(missing_ok=True)
rom=r/'out/rom.bin';rh=sha(rom)
with (o/'mame.log').open('w')as f:subprocess.run(['mame','genesis','-cart',str(rom),'-window','-nomaximize','-resolution','512x448','-video','none','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_delay','0','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],cwd=r,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=300)
assert sha(rom)==rh and sha(trace)==report['trace_sha256']and sha(r/'src/arcade_large_actor.c')==report['native_sha256']
d=json.loads((o/'results.json').read_text());d.update(host_rom_sha256=rh,native_sha256=report['native_sha256'],trace_sha256=report['trace_sha256'],code_sha256=sha(o/'code.bin'),code_bytes=(o/'code.bin').stat().st_size,scope='RAM-loaded 68000 component with test marshaling wrapper; not linked gameplay',flags=flags)
(o/'results.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2));assert d['error']is None and d['cases']==36864
