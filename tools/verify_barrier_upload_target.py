#!/usr/bin/env python3
"""Windowless SGDK cartridge: barrier uploads, layout guards and VRAM readback."""
from pathlib import Path
import argparse,hashlib,json,re,subprocess
from genplus_probe import probe
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,default=r/'src/arcade_barrier_upload.c');p.add_argument('--output-dir',type=Path,default=r/'out/parity-audit/barrier-upload-target');args=p.parse_args()
o=args.output_dir.resolve();o.mkdir(parents=True,exist_ok=True)
sdk=Path('/Users/rester159/My_apps/portForge/tools/sgdk');sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
original=sha(r/'out/rom.bin')
(o/'main.c').write_text('#include <genesis.h>\n#include "arcade_barrier_upload.h"\nvolatile u16 test_done,test_error,test_batches,test_words,test_split;\nint main(bool hard){u16 pose,k,before;u8 slot;(void)hard;VDP_setEnable(FALSE);\nVDP_setScreenWidth256();VDP_setPlaneSize(64,32,TRUE);SYS_doVBlankProcess();\narcade_barrier_upload_reset();\nif(arcade_barrier_upload(12,0,0,0) || arcade_barrier_upload(0,180,0,0))test_error=1;\nVDP_setBGAAddress(0xc000);if(arcade_barrier_upload(0,0,0,0))test_error=2;VDP_setBGAAddress(0xe000);\nif(arcade_barrier_upload(0,0,65535,0))test_error=3;\nVDP_setAutoInc(2);*((vu32*)VDP_CTRL_PORT)=VDP_WRITE_VRAM_ADDR(0xf7fe);*((vu16*)VDP_DATA_PORT)=0x1234;\n*((vu32*)VDP_CTRL_PORT)=VDP_WRITE_VRAM_ADDR(0xfe00);*((vu16*)VDP_DATA_PORT)=0x5678;\nfor(pose=0;pose<180;pose++){\n slot=pose%12;\n if(!arcade_barrier_upload(slot,pose,512,4))test_error=4;\n before=DMA_getQueueSize();if(!arcade_barrier_upload(slot,pose,512,4) || DMA_getQueueSize()!=before)test_error=5;\n SYS_doVBlankProcess();VDP_setAutoInc(2);\n *((vu32*)VDP_CTRL_PORT)=VDP_READ_VRAM_ADDR(0xf800+(u16)slot*128);\n for(k=0;k<64;k++){if(*((vu16*)VDP_DATA_PORT)!=((const u16*)arcade_barrier_pose_pixels[pose])[k])test_error=6;test_words++;}\n}\n*((vu32*)VDP_CTRL_PORT)=VDP_READ_VRAM_ADDR(0xf7fe);if(*((vu16*)VDP_DATA_PORT)!=0x1234)test_error=7;\n*((vu32*)VDP_CTRL_PORT)=VDP_READ_VRAM_ADDR(0xfe00);if(*((vu16*)VDP_DATA_PORT)!=0x5678)test_error=8;\narcade_barrier_upload_reset();if(!arcade_barrier_upload(11,179,0,0) || DMA_getQueueSize()==0)test_error=9;SYS_doVBlankProcess();\ntest_batches=3;test_split=1;test_done=1;while(1)SYS_doVBlankProcess();return 0;}\n')
flags=['-m68000','-DSGDK_GCC','-O2','-fno-builtin','-fno-strict-aliasing','-fomit-frame-pointer','-ffunction-sections','-fdata-sections','-fms-extensions','-I'+str(r/'inc'),'-I'+str(sdk/'inc'),'-I'+str(sdk/'res')]
objects=[]
for name,path in [('main',o/'main.c'),('upload',args.source),('poses',r/'src/arcade_barrier_pose_data.c')]:
 obj=o/(name+'.o');subprocess.run(['m68k-elf-gcc',*flags,'-c',str(path),'-o',str(obj)],check=True);objects.append(obj)
subprocess.run(['m68k-elf-gcc','-m68000','-n','-T',str(sdk/'md.ld'),'-nostdlib',str(r/'out/sega.o'),*map(str,objects),str(sdk/'lib/libmd.a'),'-lgcc','-Wl,--gc-sections','-o',str(o/'rom.out')],check=True)
subprocess.run(['m68k-elf-objcopy','-O','binary',str(o/'rom.out'),str(o/'rom.bin')],check=True)
b=bytearray((o/'rom.bin').read_bytes());b.extend(bytes((-len(b))%131072));b[0x18e:0x190]=(sum(int.from_bytes(b[i:i+2],'big')for i in range(0x200,len(b),2))&65535).to_bytes(2,'big');(o/'rom.bin').write_bytes(b)
nm=subprocess.check_output(['m68k-elf-nm',str(o/'rom.out')],text=True);syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',nm,re.M)}
d=probe(o/'rom.bin',syms,o)
assert sha(r/'out/rom.bin')==original
d.update(rom_sha256=sha(o/'rom.bin'),native_sha256=sha(args.source),scope='Actual68000/VDP:180 barrier uploads, all12 slots, cache reuse/reset, invalid layout/input rejection and arena sentinels. Live map publication and allocation lifetime pending.')
(o/'results.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2));assert d['done']==1 and d['error']==0 and d['batches']==3 and d['words']==11520
