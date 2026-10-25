#!/usr/bin/env python3
"""Check demo setup retention against the pinned 9492 source instructions."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess

r = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, default=r/'src/arcade_attract.c')
args = parser.parse_args()
o = r/'out/parity-audit/attract-init'
o.mkdir(parents=True, exist_ok=True)
rom = b''.join((r/'arcade'/n).read_bytes() for n in ('ss.01e', 'ss.02e', 'ss.03e'))
assert hashlib.sha256(rom).hexdigest() == '09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
# 9492..94B5: stores input at 44:45, delay at 48:49 and pointers at 4A:4D;
# no store to previous-input bytes 46:47. Pin the entire setup through RTS.
signature = bytes.fromhex('8e001e9f034f5fdd440f480f498e9a6e9f4a8e9b149f4cc601d7200c050f080f0b0f0e39')
assert rom[0x9492-0x4000:0x9492-0x4000+len(signature)] == signature
c = '''#include <assert.h>
#include <string.h>
#include "arcade_attract.h"
int main(void) {
 for(unsigned pair=0; pair<65536; ++pair) {
  ArcadeAttract s;
  memset(&s,0xa5,sizeof(s));
  s.previous[0]=pair>>8; s.previous[1]=pair;
  arcade_attract_init(&s);
  assert(s.cursor[0]==0 && s.cursor[1]==0xa6);
  assert(!s.input[0] && !s.input[1] && !s.delay[0] && !s.delay[1]);
  assert(s.previous[0]==(pair>>8) && s.previous[1]==(pair&255));
  /* 9A2C replaces previous with current at the first dispatched tick. */
  assert(arcade_attract_step(&s,pair&255));
  assert(!s.previous[0] && !s.previous[1]);
 }
 return 0;
}
'''
(o/'test.c').write_text(c)
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror',
                '-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),
                str(args.source),str(r/'src/arcade_attract_data.c'),'-o',str(o/'test')],check=True)
result = subprocess.run([str(o/'test')], capture_output=True, text=True)
report = {'passed': result.returncode == 0, 'previous_input_pairs': 65536,
          'source_sha256': hashlib.sha256(args.source.read_bytes()).hexdigest(),
          'source': str(args.source), 'stderr': result.stderr,
          'scope': 'Pinned arcade setup writes and all prior-input pairs, plus first-tick overwrite. Host ASan/UBSan; not full title lifecycle or target timing proof.'}
(o/('report.json' if args.source.resolve()==(r/'src/arcade_attract.c').resolve() else 'baseline-report.json')).write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
raise SystemExit(0 if report['passed'] else 1)
