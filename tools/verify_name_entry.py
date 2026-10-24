#!/usr/bin/env python3
"""Bounded source-contract checks for name entry; no emulator launch."""
import ctypes as C
import hashlib
import json
from pathlib import Path
import re
import subprocess

root=Path(__file__).resolve().parents[1];out=root/'out/parity-audit/name-entry'
out.mkdir(parents=True,exist_ok=True)
rom=b''.join((root/'arcade'/n).read_bytes() for n in ('ss.01e','ss.02e','ss.03e'))
assert hashlib.sha256(rom).hexdigest()=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
source=(root/'src/arcade_name_entry.c').read_text()
chars=bytes(int(v,16) for v in re.findall(r'0x([0-9a-f]+)',source.split('characters[82]={')[1].split('};')[0]))
assert chars==rom[0x6214:0x6214+82]
def pin(a,h):assert rom[a-0x4000:a-0x4000+len(bytes.fromhex(h))]==bytes.fromhex(h)
pin(0x9e62,'dc23c30d48fd03767c03782003')
pin(0x9e8f,'dc2310b3037626087c037820037a037839')
pin(0x9f75,'ae22e6022621ae26e60253e484c501270aae22e606c152250f243d')
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(root/'inc'),
 str(root/'src/arcade_name_entry.c'),'-o',str(out/'name.dylib')],check=True)
lib=C.CDLL(str(out/'name.dylib'))
class Name(C.Structure):_fields_=[('raw',C.c_uint8*12),('name',C.c_uint8*4)]
class Idle(C.Structure):_fields_=[('deadline',C.c_uint16),('phase',C.c_uint8)]
lib.arcade_name_init.argtypes=[C.POINTER(Name)]
lib.arcade_name_step.argtypes=[C.POINTER(Name),C.c_uint8,C.c_uint8]
lib.arcade_name_step.restype=C.c_uint8
lib.arcade_name_idle_step.argtypes=[C.POINTER(Idle),C.c_uint16,C.c_uint8,C.c_uint8,C.c_uint8]
lib.arcade_name_idle_step.restype=C.c_uint8
checks=0
for byte in range(256):
    s=Name((C.c_uint8*12)(*[byte]*12),(C.c_uint8*4)(0,0,0,0))
    lib.arcade_name_init(C.byref(s))
    assert list(s.raw)==[0,0,0,0,byte,byte,84,0,0,byte,byte,byte]
    assert list(s.name)==[63]*4
    checks+=1
for choice in range(85):
    for pos in range(4):
        s=Name();lib.arcade_name_init(C.byref(s));s.raw[6]=choice;s.raw[1]=pos
        s.name[:]=[17,18,19,20]
        paint=lib.arcade_name_step(C.byref(s),1,0)
        expected=[17,18,19,20];expected[pos]=chars[choice] if choice<82 else 63
        assert list(s.name)==expected
        assert s.raw[1]==(pos+1 if choice<82 and pos<3 else max(pos-1,0) if choice==82 else pos)
        assert s.raw[2]==int((choice<82 and pos==3) or choice>82)
        assert paint==(1<<pos if choice<82 else 1<<max(pos-1,0) if choice==82 else 0)
        checks+=1
        # Held fire never accepts another character.
        old=bytes(s);lib.arcade_name_step(C.byref(s),1,1);assert bytes(s)==old
        checks+=1
def move(n,bit):
    if bit==8:n=(n+2)%256;n=0 if n>84 else n
    elif bit==4:n=(n-2)%256;n=84 if n>=128 else n
    elif bit==32:n=n+17 if n+17<=84 else n
    else:n=n-17 if n>=17 else n
    if n%17==15:n+=1
    elif n%17==1:n-=1
    return n
for bit in (8,4,32,16):
    for start in range(85):
        s=Name();lib.arcade_name_init(C.byref(s));s.raw[6]=start;expected=start
        for tick in range(100):
            if tick==0 or (tick>=21 and (tick-21)%10==0):expected=move(expected,bit)
            lib.arcade_name_step(C.byref(s),bit,0 if tick==0 else bit)
            assert s.raw[6]==expected,(bit,start,tick,s.raw[6],expected)
            checks+=1
for clock in range(65536):
    s=Idle();assert not lib.arcade_name_idle_step(C.byref(s),clock,1,0,0)
    assert s.deadline==(clock+3400)&65535 and s.phase==1
    assert not lib.arcade_name_idle_step(C.byref(s),s.deadline,1,0,0) and s.phase==2
    assert lib.arcade_name_idle_step(C.byref(s),(s.deadline+1)&65535,0,0,0) and s.phase==0
    checks+=3
for two in (0,1):
    for p1 in range(64):
        for p2 in range(64):
            s=Idle(100,1);lib.arcade_name_idle_step(C.byref(s),99,p1,p2,two)
            assert s.phase==(0 if (p1|(p2 if two else 0))&60 else 1)
            checks+=1
report={'checks':checks,'passed':True,'scope':'Source-contract validation: ROM glyph table, retained initialization bytes, select/back/end, held fire, directional repeat/wrap/hole correction, all deadline wrap phases and both-controller activity masks. No original-CPU differential, sprite/map or live integration claim.'}
(out/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
