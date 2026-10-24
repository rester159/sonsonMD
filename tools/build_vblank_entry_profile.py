#!/usr/bin/env python3
"""Build isolated VBlank-arrival histogram instrumentation; no wait-policy change."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess

root=Path(__file__).resolve().parents[1]
dest=root/'out/vblank-entry-profile'
dest.mkdir(exist_ok=True)
for name in ('src','inc'):
    shutil.copytree(root/name,dest/name,dirs_exist_ok=True)
(dest/'tools').mkdir(exist_ok=True)
for name in ('build_native.py','verify_second_core.py','verify_libretro.c'):
    shutil.copy2(root/'tools'/name,dest/'tools'/name)
path=dest/'src/game.c'
source=path.read_text()
marker='        g_presentCounter++;   /* prepared display, possibly repeated simulation */\n        SYS_doVBlankProcess();'
assert source.count(marker)==1
source=source.replace(marker,'''        g_presentCounter++;   /* prepared display, possibly repeated simulation */
        {
            /* Reads are sequential, not an atomic raster/IRQ snapshot. */
            u16 line=GET_VCOUNTER;
            u16 blank=GET_VDP_STATUS(VDP_VBLANK_FLAG)?256:0;
            u16 bytes=DMA_getQueueTransferSize();
            volatile u32 *row=vblank_arrivals[line+blank];
            row[0]++;row[1]+=bytes;if(bytes>row[2])row[2]=bytes;
        }
        SYS_doVBlankProcess();''')
source=source.replace('void game_run(void)\n{','volatile u32 vblank_arrivals[512][3];\nvoid game_run(void)\n{')
path.write_text(source)
path=dest/'tools/verify_libretro.c'
source=path.read_text();marker='    if(getenv("SONSON_VRAM_DUMP"))'
assert source.count(marker)==1
source=source.replace(marker,'''    if(getenv("SONSON_WAIT_PROFILE")) {
        unsigned addr=strtoul(getenv("SONSON_WAIT_PROFILE"),NULL,16);
        for(unsigned k=0;k<512;k++)if(r32(ram,addr+k*12))
            printf("WAIT_ENTRY %u %u %u %u\\n",k,r32(ram,addr+k*12),r32(ram,addr+k*12+4),r32(ram,addr+k*12+8));
    }
'''+marker)
path.write_text(source)
path=dest/'tools/verify_second_core.py'
source=path.read_text().replace('env=os.environ.copy();','env=os.environ.copy();env["SONSON_WAIT_PROFILE"]=syms["vblank_arrivals"];')
path.write_text(source)
subprocess.run(['python3',str(dest/'tools/build_native.py')],check=True)
(dest/'profile-scope.json').write_text(json.dumps({
    'production_rom_sha256':hashlib.sha256((root/'out/rom.bin').read_bytes()).hexdigest(),
    'scope':'All game_run presentation boundaries since boot; sequential raster and blank-flag reads, then queued DMA bytes. No pause samples. Added RAM/counter overhead perturbs timing. Does not measure DMA completion, tearing, audio service or prove that late-VBlank processing is safe.',
    'layout':'512 rows of three u32 words: count, sum queued bytes, max queued bytes. Index is raw V counter plus 256 when blank flag set. Raw V counter rolls back during NTSC VBlank; do not interpret it as a linear full-frame scanline.'
},indent=2)+'\n')
print('Diagnostic only:',dest/'out/rom.bin')
