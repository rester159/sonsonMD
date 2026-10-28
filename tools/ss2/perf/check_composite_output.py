#!/usr/bin/env python3
"""Check an SS2_COMPCHECK ROM against its source-format sprite decoder.

Usage: check_composite_output.py BUILD_DIR EVIDENCE_DIR [REFRESHES]
Uses the existing Genesis Plus GX harness; no physical-hardware claim.
"""
import json
import os
from pathlib import Path
import re
import subprocess
import sys

build, dest = (Path(p).resolve() for p in sys.argv[1:3])
dest.mkdir(parents=True, exist_ok=True)
symbols = {n: int(a, 16) & 65535 for a, n in re.findall(
    r'^([0-9a-f]+)\s+\w\s+(\S+)$', (build / 'symbol.txt').read_text(), re.M)}
address = symbols['ss2_compchk']
env = dict(os.environ, SS2_RAMDUMP=str(dest / 'ram.bin'))
subprocess.run([sys.executable, str(Path(__file__).with_name('benchmark.py')),
                str(build), str(dest), sys.argv[3] if len(sys.argv) > 3 else '3500'],
               env=env, check=True, timeout=120)
ram = (dest / 'ram.bin').read_bytes()
def read32(offset):
    return int.from_bytes(bytes(ram[((offset + i) & 65535) ^ 1] for i in range(4)), 'big')
report = {'composites_checked': read32(address), 'differences': read32(address + 4)}
assert report['composites_checked'] >= 1000, report
assert report['differences'] == 0, report
(dest / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
