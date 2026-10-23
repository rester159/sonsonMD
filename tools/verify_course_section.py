#!/usr/bin/env python3
"""Offline descriptor-zero startup contract with poisoned previous state."""
from pathlib import Path
import ctypes as C
import subprocess,json
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/course-section';o.mkdir(parents=True,exist_ok=True)
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-dynamiclib','-I'+str(r/'inc'),str(r/'src/arcade_section.c'),str(r/'src/arcade_section_data.c'),'-o',str(o/'test.dylib')],check=True)
class Section(C.Structure):
 _fields_=[('speed',C.c_uint16),('checkpoint',C.c_uint16)]+[(n,C.c_uint8)for n in ('index','remaining','column','difficulty','stop','next_stop','timer','next_timer')]
lib=C.CDLL(str(o/'test.dylib'));lib.arcade_section_course_begin.argtypes=[C.POINTER(Section),C.c_uint8]
for difficulty in range(256):
 for old in range(256):
  s=Section(0xa500|old,65535,25,99,31,0,2,2,42,old)
  lib.arcade_section_course_begin(C.byref(s),difficulty)
  assert [getattr(s,n)for n,_ in Section._fields_]==[(difficulty<<8)|old,0,0,4,0,difficulty,0,0,old,0]
report={'cases':65536,'scope':'Descriptor0 course-start offline contract: clears stop/column, retains speed low byte, copies old next timer, reloads first descriptor. Other descriptors and source execution not tested.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
