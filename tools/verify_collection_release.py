#!/usr/bin/env python3
"""Check the shipping ROM identity, header, checksum, and diagnostic-free build."""
from pathlib import Path
import hashlib,json,re
root=Path(__file__).resolve().parents[1]
version=re.search(r'#define SONSON_VERSION "([^"]+)"',(root/'inc/release_version.h').read_text()).group(1)
rom=root/'dist'/f'SonSonCollection-v{version}.bin'
b=rom.read_bytes(); manifest=json.loads((root/'out/build-manifest.json').read_text())
assert b==(root/'out/rom.bin').read_bytes(), 'dist differs from the linked build'
assert len(b)<=0x400000 and len(b)%131072==0
assert b[0x100:0x110].decode().strip()=='SEGA MEGA DRIVE'
for start in (0x120,0x150):
 assert b[start:start+48].decode().strip()==f'SONSON COLLECTION V{version}'
assert int.from_bytes(b[0x1a0:0x1a4],'big')==0
assert int.from_bytes(b[0x1a4:0x1a8],'big')==len(b)-1
assert int.from_bytes(b[4:8],'big')<len(b)
checksum=sum(int.from_bytes(b[i:i+2],'big') for i in range(0x200,len(b),2))&65535
assert int.from_bytes(b[0x18e:0x190],'big')==checksum
assert b[0x1b0:0x1b2]==b'RA' and b[0x1f0:0x1f3]==b'JUE'
assert not any(f.startswith('-DSS2_') for f in manifest['flags']), 'diagnostic build'
sha=hashlib.sha256(b).hexdigest()
assert manifest['version']==version and manifest['rom_sha256']==sha
for file,want in manifest['source_sha256'].items():
 assert hashlib.sha256((root/file).read_bytes()).hexdigest()==want, f'stale build: {file}'
print(json.dumps({'version':version,'file':rom.name,'bytes':len(b),'sha256':sha,'checksum':f'{checksum:04x}','header_and_sources':'pass'},indent=2))
