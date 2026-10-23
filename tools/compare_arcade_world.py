#!/usr/bin/env python3
"""Independent source-CPU coverage of all original terrain/macro columns."""
from pathlib import Path
import csv,json,hashlib
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit'
b=b''.join((r/'arcade'/f).read_bytes()for f in ['ss.01e','ss.02e','ss.03e'])
rd=lambda a:b[a-0x4000];r16=lambda a:rd(a)*256+rd(a+1)
rows=list(csv.DictReader((o/'world-oracle.csv').open()));seen=set();checks=0;bad=[]
for row in rows:
 stage=int(row['stage']);col=int(row['col']);rec=0x6485+stage*7;a=r16(rec+3)+(col//2)*13;seen.add((stage,col))
 codes=[16];attrs=[0]
 for k in range(13):
  macro=0x653b+rd(a+k)*8+(col%2)*2;codes.extend([rd(macro),rd(macro+1)]);attrs.extend([rd(macro+4),rd(macro+5)])
 expected={'codes':bytes(codes),'attributes':bytes(attrs),'collision':bytes(rd(r16(rec+5)+col*4+i)for i in range(4))}
 for key,exp in expected.items():
  act=bytes.fromhex(row[key]);checks+=len(exp)
  if exp!=act:bad.append({'stage':stage,'col':col,'field':key,'expected':exp.hex(),'actual':act.hex()})
result={'program_sha256':hashlib.sha256(b).hexdigest(),'covered_segment_columns':len(seen),'expected_segment_columns':26*32,'bytes_compared':checks,'mismatches':bad,'fixture':'Original CPU column expansion; accelerated scroll and selected segment pointers, not a natural playthrough'}
(o/'world-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));assert len(rows)==832 and len(seen)==832 and not bad
