#!/usr/bin/env python3
"""Original large firing fixtures on a RAM-loaded native 68000 component."""
import hashlib,json,re,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/k-projectile-target';o.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
trace=r/'out/parity-audit/k-projectile-oracle.csv';report=json.loads((r/'out/parity-audit/k-projectile-comparison.json').read_text())
assert sha(trace)==report['trace_sha256'] and sha(r/'src/arcade_k.c')==report['native_sha256']
cases=r/'out/parity-audit/k-projectile-target.csv';assert sha(cases)==report['target_sha256'];(o/'cases.csv').write_bytes(cases.read_bytes())
(o/'wrapper.c').write_text('#include "arcade_k.h"\nvoid test_large(const u8 *v,u8 *out){ArcadeKProjectile a;\na.x=(u16)((v[0]<<8)|v[1]);a.origin=(u16)((v[2]<<8)|v[3]);\na.xf=v[4];a.y=v[5];a.yf=v[6];a.active=v[7];a.state=v[8];a.phase=v[9];a.facing=v[10];a.death_timer=v[11];a.cursor=v[12];a.anim_timer=v[13];a.code=v[14];a.attr=v[15];\narcade_k_projectile_step(&a,(u16)((v[16]<<8)|v[17]),v[18],v[19]);\nout[0]=(u8)(a.x>>8);out[1]=(u8)a.x;out[2]=(u8)(a.origin>>8);out[3]=(u8)a.origin;\nout[4]=a.xf;out[5]=a.y;out[6]=a.yf;out[7]=a.active;out[8]=a.state;out[9]=a.phase;out[10]=a.facing;out[11]=a.death_timer;out[12]=a.cursor;out[13]=a.anim_timer;out[14]=a.code;out[15]=a.attr;\n}\n')
sdk=Path('/Users/rester159/My_apps/portForge/tools/sgdk');cc='/opt/homebrew/bin/m68k-elf-gcc'
flags=['-m68000','-DSGDK_GCC','-O2','-fno-builtin','-fomit-frame-pointer','-fms-extensions','-I'+str(r/'inc'),'-I'+str(sdk/'inc'),'-I'+str(sdk/'res')]
for name,source in [('native',r/'src/arcade_k.c'),('wrapper',o/'wrapper.c')]:subprocess.run([cc,*flags,'-c',str(source),'-o',str(o/(name+'.o'))],check=True)
subprocess.run(['m68k-elf-ld','-Ttext=0xff9000','--entry=test_large',str(o/'native.o'),str(o/'wrapper.o'),'-o',str(o/'code.elf')],check=True)
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
   for k=0,15 do assert(m:read_u8(0xffb300+k)==tonumber(pending:sub(k*2+1,k*2+2),16),'case '..count..' output '..k)end
   count=count+1
  else for k=1,#binary do m:write_u8(0xff9000+k-1,binary:byte(k))end end
  local line=input:read('*l');if not line then finish();return end
  local src,expected=line:match('([^,]+),([^,]+)');pending=expected
  for k=0,#src//2-1 do m:write_u8(0xffb200+k,tonumber(src:sub(k*2+1,k*2+2),16))end
  for k=0,15 do m:write_u8(0xffb300+k,0xa5)end
  for a=0xffb000,0xffb03e,2 do m:write_u16(a,0x4e71)end
  m:write_u16(0xffb040,0x4eb9);m:write_u32(0xffb042,ENTRY);m:write_u16(0xffb046,0x60fe)
  m:write_u32(0xffbfe0,0xffb200);m:write_u32(0xffbfe4,0xffb300)
  cpu.state['SR'].value=0x2700;cpu.state['SP'].value=0xffbfe0;cpu.state['PC'].value=0xffb000
 end)
 if not ok then finish(err)end
end)
'''
for k,v in {'INPUT':str(o/'cases.csv'),'CODE':str(o/'code.bin'),'RESULT':str(o/'results.json')}.items():s=re.sub(r'\b'+k+r'\b',json.dumps(v),s)
s=s.replace('ENTRY',str(entry));(o/'run.lua').write_text(s);(o/'results.json').unlink(missing_ok=True)
rom=r/'out/rom.bin';rh=sha(rom)
with (o/'mame.log').open('w')as f:subprocess.run(['mame','genesis','-cart',str(rom),'-window','-nomaximize','-resolution','512x448','-video','none','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_delay','0','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],cwd=r,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=300)
assert sha(rom)==rh and sha(trace)==report['trace_sha256']and sha(r/'src/arcade_k.c')==report['native_sha256']
d=json.loads((o/'results.json').read_text());d.update(host_rom_sha256=rh,native_sha256=report['native_sha256'],trace_sha256=report['trace_sha256'],code_sha256=sha(o/'code.bin'),code_bytes=(o/'code.bin').stat().st_size,scope='RAM-loaded 68000 component with test marshaling wrapper; not linked gameplay',flags=flags)
(o/'results.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2));assert d['error']is None and d['cases']==23040
