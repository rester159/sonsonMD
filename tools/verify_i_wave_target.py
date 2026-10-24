#!/usr/bin/env python3
"""Run compiled class-I controller in Genesis RAM; not production integration."""
from pathlib import Path
import hashlib,json,re,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/i-wave-target';o.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
trace=r/'out/parity-audit/i-wave-target.csv';report=json.loads((r/'out/parity-audit/i-wave-comparison.json').read_text());identity=sha(trace);assert identity==report['target_sha256']
source=r/'src/arcade_i.c';assert sha(source)==report['native_sha256']
rom=r/'out/rom.bin';rh=sha(rom)
sdk=Path('/Users/rester159/My_apps/portForge/tools/sgdk')
cc='/opt/homebrew/bin/m68k-elf-gcc';ld='/opt/homebrew/bin/m68k-elf-ld'
flags=['-m68000','-DSGDK_GCC','-O2','-fno-builtin','-fomit-frame-pointer','-ffunction-sections','-fdata-sections','-fms-extensions','-I'+str(r/'inc'),'-I'+str(sdk/'inc'),'-I'+str(sdk/'res')]
subprocess.run([cc,*flags,'-c',str(source),'-o',str(o/'controller.o')],check=True)
subprocess.run([ld,'-Ttext=0xffb800','--entry=arcade_i_wave_step','--gc-sections',str(o/'controller.o'),'-o',str(o/'controller.elf')],check=True)
subprocess.run(['/opt/homebrew/bin/m68k-elf-objcopy','-O','binary',str(o/'controller.elf'),str(o/'controller.bin')],check=True)
code=(o/'controller.bin').read_bytes();assert len(code)<1024
nm=subprocess.check_output(['/opt/homebrew/bin/m68k-elf-nm',str(o/'controller.elf')],text=True)
address=int(re.search(r'^([0-9a-fA-F]+) T arcade_i_wave_step$',nm,re.M)[1],16);assert address==0xffb800
s='''local cpu=manager.machine.devices[":maincpu"];local m=cpu.spaces.program
local input=assert(io.open(TRACE,"r"));input:read("*l")
local code=assert(io.open(CODE,"rb"));local binary=code:read("*a");code:close()
local frame=0;local pending=nil;local terminal=false;local cases=0
local function finish(err)
 terminal=true;input:close();local f=assert(io.open(RESULT,"w"));f:write(string.format('{"cases":%d,"field_checks":%d,"error":%s}',cases,cases*16,err and string.format('%q',err)or"null"));f:close();manager.machine:exit()
end
local function bytes(a,s)
 for k=0,#s//2-1 do m:write_u8(a+k,tonumber(s:sub(k*2+1,k*2+2),16))end
end
local function compare(a,s,skip)
 for k=0,#s//2-1 do if k~=skip then assert(m:read_u8(a+k)==tonumber(s:sub(k*2+1,k*2+2),16),string.format('case %d address %x',cases,a+k))end end
end
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1;if frame<60 then return end
 local ok,err=pcall(function()
  if pending then
   assert(cpu.state["CURPC"].value&0xffffff==0xffb046,"routine did not return")
   compare(0xffb200,pending[6],7);compare(0xffb300,pending[9])
   assert(m:read_u8(0xffb280)==pending[7],'reservation '..cases)
   assert(cpu.state["D0"].value&255==pending[8],'birth flag '..cases)
   cases=cases+1
  else
   for k=1,#binary do m:write_u8(0xffb800+k-1,binary:byte(k))end
  end
  local line=input:read("*l");if not line then finish();return end
  local d={}for n in line:gmatch("[^,]+")do d[#d+1]=n end
  for _,k in ipairs({2,3,4,5,7,8})do d[k]=tonumber(d[k])end;pending=d
  bytes(0xffb200,d[1]);m:write_u8(0xffb280,d[5]);bytes(0xffb300,'5678abcdef')
  for x=0xffb000,0xffb03e,2 do m:write_u16(x,0x4e71)end
  m:write_u16(0xffb040,0x4eb9);m:write_u32(0xffb042,0xffb800);m:write_u16(0xffb046,0x60fe)
  m:write_u32(0xffbfe0,0xffb200);m:write_u32(0xffbfe4,d[2]);m:write_u32(0xffbfe8,d[3]);m:write_u32(0xffbfec,d[4]);m:write_u32(0xffbff0,0xffb280);m:write_u32(0xffbff4,0xffb300)
  cpu.state["SR"].value=0x2700;cpu.state["SP"].value=0xffbfe0;cpu.state["PC"].value=0xffb000
 end)
 if not ok then finish(err)end
end)
'''
for k,v in {'TRACE':str(trace),'RESULT':str(o/'results.json'),'CODE':str(o/'controller.bin')}.items():s=re.sub(r'\b'+k+r'\b',json.dumps(v),s)
(o/'run.lua').write_text(s)
subprocess.run(['mame','genesis','-cart',str(rom),'-window','-nomaximize','-resolution','512x448','-video','none','-keyboardprovider','none','-mouseprovider','none','-joystickprovider','none','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],cwd=r,stdout=(o/'mame.log').open('w'),stderr=subprocess.STDOUT,check=True,timeout=300)
assert sha(rom)==rh and sha(trace)==identity and sha(source)==report['native_sha256']
result=json.loads((o/'results.json').read_text());result.update(scope='RAM-loaded native component, not linked gameplay',host_rom_sha256=rh,code_sha256=sha(o/'controller.bin'),source_sha256=sha(source),trace_sha256=identity,compiler=subprocess.check_output([cc,'--version'],text=True).splitlines()[0],flags=flags,code_bytes=len(code))
(o/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));assert result['error']is None and result['cases']==8640,result
