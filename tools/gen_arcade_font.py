#!/usr/bin/env python3
"""Rebuild the original arcade ASCII font in arcade_ui.c from ss_title.c."""
from pathlib import Path
import re
r=Path(__file__).resolve().parents[1]
words=[int(x,16) for x in re.findall(r'0x([0-9A-F]{8})', (r/'src/ss_title.c').read_text().split('const u16')[0])]
tiles=[]
extra={':':[0,0x18,0x18,0,0,0x18,0x18,0], ',':[0,0,0,0,0,0x18,0x18,0x30], '>':[0,0x60,0x30,0x18,0x30,0x60,0,0], '<':[0,0x0c,0x18,0x30,0x18,0x0c,0,0], '/':[2,6,12,24,48,96,192,0], '+':[0,0x18,0x18,0x7e,0x18,0x18,0,0], '-':[0,0,0,0x7e,0,0,0,0], '?':[0x3c,0x66,6,12,24,0,24,0]}
for a in range(32,128):
 ch=chr(a).upper(); code=int(ch) if ch.isdigit() else 17+ord(ch)-65 if 'A'<=ch<='Z' else 16
 src=words[(149+code)*8:(150+code)*8]
 # All nonzero original ink becomes SDK-compatible index 15.
 t=[sum((15 if (w>>(4*b))&15 else 0)<<(4*b) for b in range(8)) for w in src]
 if ch in extra: t=[sum((15 if bits&(128>>b) else 0)<<(28-4*b) for b in range(8)) for bits in extra[ch]]
 tiles.extend(t)
p=r/"src/arcade_ui.c"
s=p.read_text()
a=s.index("static const u32 font[96*8] = {\n")+len("static const u32 font[96*8] = {\n")
b=s.index("};",a)
data="".join("    "+",".join("0x%08X"%w for w in tiles[i:i+8])+",\n" for i in range(0,len(tiles),8))
p.write_text(s[:a]+data+s[b:])
