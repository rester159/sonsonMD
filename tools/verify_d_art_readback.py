#!/usr/bin/env python3
"""Check the optional verify_libretro VRAM dump after a windowless game run."""
from pathlib import Path
import re,json,hashlib
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/d-art'
v=(o/'vram.bin').read_bytes();assert len(v)==65568
s=(r/'src/arcade_d_art.c').read_text()
w=[int(n,16)for n in re.findall(r'0x([0-9a-fA-F]{8})',s.split('pixels',1)[1].split('};',1)[0])]
expected=b''.join(n.to_bytes(4,'big')for n in w)
actual=v[0xf020:0xf320];swapped=b''.join(actual[i:i+2][::-1]for i in range(0,len(actual),2))
assert swapped==expected
regs=v[65536:];assert regs[11]&7==0 and regs[13]==0x3c and regs[5]==0x7a
runtime=json.loads((r/'out/genplus-results.json').read_text())[0]
assert runtime['rom_sha256']==hashlib.sha256((r/'out/rom.bin').read_bytes()).hexdigest()
report={'tile_bytes_verified':len(expected),'address_start':'F020','address_end_exclusive':'F320','scroll_mode':regs[11]&7,'hscroll_register':regs[13],'sat_register':regs[5],'vram_word_order':'swapped','rom_sha256':runtime['rom_sha256'],'runtime_ticks':runtime['simulation_elapsed'],'scope':'Genesis Plus GX VRAM readback and table settings; not D gameplay or original hardware validation. Dump must come from the recorded run.'}
(o/'readback.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
