#!/usr/bin/env python3
"""Fail if the linked ROM contains an M68000 move whose source post-increments
or pre-decrements an address register that the destination also uses.

GCC 16.1 (m68k-elf, -O3 -flto) emits e.g. `move.b (a0)+,(0,a0,d0.l)` for
byte-copy loops; the 68000 updates a0 before computing the destination, so
every byte is stored one address too high. Usage: check_autoinc_hazard.py ELF
"""
import re, subprocess, sys
dis = subprocess.run(['m68k-elf-objdump', '-d', sys.argv[1]], capture_output=True, text=True, check=True).stdout
# source (aN)+ or -(aN); destination d(aN) / d(aN,Xn) (base computed after
# the update). Symbols that are embedded data (Z80 drivers) are skipped.
pat = re.compile(r'\tmove[bwl]?\s+(?:%a([0-7])@\+|%a([0-7])@-),(%a[0-7]@\(.*)$')
DATA = ('drv_', 'z80_drv', 'MDDBG__')
fn, bad = None, []
for line in dis.splitlines():
    m = re.match(r'^[0-9a-f]+ <(.+)>:', line)
    if m:
        fn = m.group(1)
        continue
    m = pat.search(line)
    if m and not (fn or '').startswith(DATA):
        reg = m.group(1) or m.group(2)
        if re.search(r'%a' + reg + r'\b', m.group(3)):
            bad.append((fn, line.strip()))
for f, l in bad:
    print('auto-increment hazard in %s: %s' % (f, l))
sys.exit(1 if bad else 0)
