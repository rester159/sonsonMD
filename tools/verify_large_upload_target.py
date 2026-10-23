#!/usr/bin/env python3
"""Standalone SGDK cartridge: actual queued ROM DMA and VDP VRAM readback."""
from pathlib import Path
import hashlib,json,re,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/large-upload-target';o.mkdir(parents=True,exist_ok=True)
sdk=Path('/Users/rester159/My_apps/portForge/tools/sgdk');sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
original=sha(r/'out/rom.bin')
(o/'main.c').write_text('''#include <genesis.h>
#include "arcade_large_upload.h"
#include "arcade_large_present.h"
volatile u16 test_done,test_error,test_batches,test_words,test_split;
static u16 frames[16],sat[64*4];
static ArcadeLargeCell cells[16];
static const ArcadeSpriteView view={0,0,256,224};
static u16 test_present(void)
{
 u16 k,n;u32 command;
 for(k=0;k<256;k++)sat[k]=0xa5a5;
 for(k=0;k<16;k++){cells[k].x=32+(k%8)*24;cells[k].y=40+(k/8)*32;cells[k].code=0x89;cells[k].attr=0xe4;}
 arcade_large_upload_reset();DMA_setMaxTransferSize(1);
 if(arcade_large_present(cells,0,&view,sat,2,64,65535,65535)!=0 || DMA_getQueueSize())return 53;
 if(arcade_large_present(cells,0,&view,sat,65,64,0,0)!=65535)return 54;
 if(arcade_large_present(cells,65535,&view,sat,2,64,704,2)!=65535 || DMA_getQueueSize())return 40;
 for(k=0;k<256;k++)if(sat[k]!=0xa5a5)return 41;
 DMA_setMaxTransferSize(4096);
 n=arcade_large_present(cells,65535,&view,sat,2,64,704,2);
 if(n!=16)return 42;
 for(k=0;k<16;k++){
  u16 off=(k+2)*4;
  if(sat[off]!=128+cells[k].y || sat[off+1]!=(0x500|(k+3)) || sat[off+2]!=(0x2000|1308+k*4) || sat[off+3]!=128+cells[k].x)return 43;
 }
 if(sat[0]!=0xa5a5 || sat[72]!=0xa5a5)return 44;
 if(!DMA_queueDma(DMA_VRAM,sat+8,VDP_getSpriteListAddress()+16,64,2))return 45;
 SYS_doVBlankProcess();VDP_setAutoInc(2);
 command=VDP_READ_VRAM_ADDR(VDP_getSpriteListAddress()+16);*((vu32*)VDP_CTRL_PORT)=command;
 for(k=0;k<64;k++)if(*((vu16*)VDP_DATA_PORT)!=sat[8+k])return 46;
 if(arcade_large_present(cells,65535,&view,sat,2,64,704,2)!=16 || DMA_getQueueSize())return 47;
 cells[0].x=250;cells[0].y=40;cells[0].attr=0xa4;
 if(arcade_large_present(cells,1,&view,sat,2,64,704,2)!=2)return 48;
 if(sat[11]!=378 || sat[15]!=122 || sat[10]!=(0x2800|1308))return 49;
 cells[0].y=250;
 if(arcade_large_present(cells,1,&view,sat,2,64,704,2)!=1)return 50;
 /* Only independent Y-wrap is visible at x250; no diagonal wrap copy. */
 if(sat[8]!=122 || sat[11]!=378)return 51;
 cells[0].code=0xff;cells[0].attr=0;
 {u16 old=sat[8];if(arcade_large_present(cells,1,&view,sat,2,64,704,2)!=65535 || sat[8]!=old)return 52;}
 return 0;
}
static u16 verify(void)
{
 u16 k,j;VDP_setAutoInc(2);
 for(k=0;k<16;k++){
  const u16 *expected=(const u16*)(arcade_large_art_pixels+(u32)frames[k]*32);
  *((vu32*)VDP_CTRL_PORT)=VDP_READ_VRAM_ADDR((1308+k*4)*32);
  for(j=0;j<64;j++){u16 got=*((vu16*)VDP_DATA_PORT);if(got!=expected[j])return 1;test_words++;}
 }
 *((vu32*)VDP_CTRL_PORT)=VDP_READ_VRAM_ADDR(1308*32-2);
 if(*((vu16*)VDP_DATA_PORT)!=0x1234)return 2;
 *((vu32*)VDP_CTRL_PORT)=VDP_READ_VRAM_ADDR(1372*32);
 if(*((vu16*)VDP_DATA_PORT)!=0x5678)return 3;
 return 0;
}
int main(bool hard)
{
 u16 batch,k,err;(void)hard;VDP_setScreenWidth256();VDP_setPlaneSize(64,32,TRUE);
 SYS_doVBlankProcess();DMA_setMaxQueueSize(40);DMA_setMaxTransferSize(4096);
 DMA_setIgnoreOverCapacity(TRUE);arcade_large_upload_reset();VDP_setAutoInc(2);
 *((vu32*)VDP_CTRL_PORT)=VDP_WRITE_VRAM_ADDR(1308*32-2);*((vu16*)VDP_DATA_PORT)=0x1234;
 *((vu32*)VDP_CTRL_PORT)=VDP_WRITE_VRAM_ADDR(1372*32);*((vu16*)VDP_DATA_PORT)=0x5678;
 for(k=0;k<16;k++)frames[k]=k;
 DMA_setMaxTransferSize(2047);
 if(arcade_large_upload(frames,0,0)||DMA_getQueueSize()){test_error=10;goto done;}
 DMA_setMaxTransferSize(4096);
 for(batch=0;batch<12;batch++){
  for(k=0;k<16;k++)frames[k]=(batch*16+k)%ARCADE_LARGE_ART_FRAMES;
  if(!arcade_large_upload(frames,704,2)){test_error=11;goto done;}
  if(DMA_getQueueTransferSize()!=2048){test_error=12;goto done;}
  if(batch==0){test_split=DMA_getQueueSize();if(test_split!=17){test_error=13;goto done;}}
  SYS_doVBlankProcess();err=verify();if(err){test_error=20+err;goto done;}
  if(!arcade_large_upload(frames,704,2)||DMA_getQueueSize()){test_error=30;goto done;}
  test_batches++;
 }
 test_error=test_present();
 done:test_done=1;while(1)SYS_doVBlankProcess();return 0;
}
''')
# Deliberately cross a real 128 KiB ROM DMA boundary in frame zero.
s=(r/'src/arcade_large_art.c').read_text().replace('const u32 arcade_large_art_pixels[','const u32 __attribute__((section(".largeart"))) arcade_large_art_pixels[')
(o/'art.c').write_text(s)
flags=['-m68000','-DSGDK_GCC','-O2','-fno-builtin','-fomit-frame-pointer','-ffunction-sections','-fdata-sections','-fms-extensions','-I'+str(r/'inc'),'-I'+str(sdk/'inc'),'-I'+str(sdk/'res')]
objects=[]
for name,path in [('main',o/'main.c'),('art',o/'art.c'),('upload',r/'src/arcade_large_upload.c'),('sat',r/'src/arcade_cell_sat.c'),('present',r/'src/arcade_large_present.c')]:
 obj=o/(name+'.o');subprocess.run(['m68k-elf-gcc',*flags,'-c',str(path),'-o',str(obj)],check=True);objects.append(obj)
subprocess.run(['m68k-elf-gcc','-m68000','-n','-T',str(sdk/'md.ld'),'-nostdlib',str(r/'out/sega.o'),*map(str,objects),str(sdk/'lib/libmd.a'),'-lgcc','-Wl,--gc-sections','-Wl,--section-start=.largeart=0x1ffc0','-o',str(o/'rom.out')],check=True)
subprocess.run(['m68k-elf-objcopy','-O','binary',str(o/'rom.out'),str(o/'rom.bin')],check=True)
b=bytearray((o/'rom.bin').read_bytes());b.extend(bytes((-len(b))%131072));b[0x18e:0x190]=(sum(int.from_bytes(b[i:i+2],'big')for i in range(0x200,len(b),2))&65535).to_bytes(2,'big');(o/'rom.bin').write_bytes(b)
nm=subprocess.check_output(['m68k-elf-nm',str(o/'rom.out')],text=True);syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',nm,re.M)}
assert syms['arcade_large_art_pixels']==0x1ffc0
s='''local m=manager.machine.devices[":maincpu"].spaces.program;local frame=0;local terminal=false
emu.register_frame_done(function()
 if terminal then manager.machine:exit();return end;frame=frame+1
 if m:read_u16(DONE)==1 or frame>600 then
  terminal=true;local f=assert(io.open(RESULT,'w'));f:write(string.format('{"done":%d,"error":%d,"batches":%d,"words":%d,"split_descriptors":%d}',m:read_u16(DONE),m:read_u16(ERROR),m:read_u16(BATCHES),m:read_u16(WORDS),m:read_u16(SPLIT)));f:close();manager.machine:exit()
 end
end)
'''
for key,name in [('DONE','test_done'),('ERROR','test_error'),('BATCHES','test_batches'),('WORDS','test_words'),('SPLIT','test_split')]:s=re.sub(r'\b'+key+r'\b',str(syms[name]),s)
s=s.replace('RESULT',json.dumps(str(o/'results.json')));(o/'run.lua').write_text(s);(o/'results.json').unlink(missing_ok=True)
with(o/'mame.log').open('w')as f:subprocess.run(['mame','genesis','-cart',str(o/'rom.bin'),'-window','-nomaximize','-resolution','512x448','-video','none','-noreadconfig','-sound','none','-nothrottle','-skip_gameinfo','-autoboot_delay','0','-autoboot_script',str(o/'run.lua'),'-cfg_directory',str(o/'cfg'),'-nvram_directory',str(o/'nvram')],cwd=r,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=90)
assert sha(r/'out/rom.bin')==original
d=json.loads((o/'results.json').read_text());d.update(rom_sha256=sha(o/'rom.bin'),native_sha256=sha(r/'src/arcade_large_upload.c'),present_sha256=sha(r/'src/arcade_large_present.c'),sat_sha256=sha(r/'src/arcade_cell_sat.c'),scope='Standalone target cartridge, actual SGDK queue/VBlank, graphics and composed SAT readback, admission/reuse/wrap; no gameplay or audio stress')
(o/'results.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2));assert d['done']==1 and d['error']==0 and d['batches']==12 and d['words']==12288 and d['split_descriptors']==17
