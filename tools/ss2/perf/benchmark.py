#!/usr/bin/env python3
"""Run a built SS2 diagnostic ROM: BUILD_DIR EVIDENCE_DIR [REFRESHES].

Run from the repository root. Compile tools/ss2/gpgx_shot.c into
out/ss2/gpgx_shot first. With SS2_PERF_INPUT, input/life are driven by the
actual simulation tick and work.csv publishes stable completed state.
Other builds use the legacy host-refresh input fixture; do not treat
those routes as identical just because they share an input script.
"""
import os,sys,re,subprocess,json
from pathlib import Path
root=Path.cwd(); build=root/sys.argv[1]; dest=root/sys.argv[2];dest.mkdir(parents=True,exist_ok=True)
sym={n:int(a,16) for a,n in re.findall(r'^([0-9a-f]+)\s+\w\s+(\S+)$',(build/'symbol.txt').read_text(),re.M)}
def addr(n):
 return next((a for k,a in sym.items() if k==n or k.startswith(n+'.')),0)
env=dict(os.environ,SS2_TRACE=str(dest/'trace.csv'),SS2_AUTOPLAY_LOOP='1000',SS2_AUTOPLAY_ABS='60',SS2_LIFE='1')
if '-DSS2_PERF_INPUT' in json.loads((build/'build-manifest.json').read_text())['flags']:
 for k in ('SS2_LIFE','SS2_AUTOPLAY_LOOP','SS2_AUTOPLAY_ABS'):env.pop(k,None)
for e,s in [('LOOP','ss2_loop_n'),('RAM','ss2_ram'),('PRES','ss2_present_n'),('COUNT','ss2_tc_last'),('PROF','ss2_tp_last')]:
 if addr(s):env['SS2_TRACE_'+e]=hex(addr(s))
if addr('ss2_pc_ring'):
 env['SS2_TRACE_PCRING']=f"{addr('ss2_pc_ring'):x},{addr('ss2_pc_i'):x}";env['SS2_PCOUT']=str(dest/'pc.txt')
if addr('ss2_perf_work'):
 env['SS2_TRACE_WORK']=f"{addr('ss2_perf_work'):x},{addr('ss2_perf_index'):x}"
 env['SS2_WORKOUT']=str(dest/'work.csv')
with (dest/'run.log').open('w') as f:
 subprocess.run([str(root/'out/ss2/gpgx_shot'),str(Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'),str(build/'rom.bin'),sys.argv[3] if len(sys.argv)>3 else '7800',str(dest),'60-68:S;120-128:R;180-188:S','2000,5000,7799'],env=env,stdout=f,stderr=subprocess.STDOUT,check=True)
print(dest)
