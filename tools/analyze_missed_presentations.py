#!/usr/bin/env python3
"""Attribute refresh-boundary PC samples; these are not cycle measurements."""
from pathlib import Path
import argparse,bisect,collections,csv,hashlib,json,re
p=argparse.ArgumentParser();p.add_argument('directory',type=Path);a=p.parse_args();root=a.directory
symbols=sorted((int(address,16),name) for address,kind,name in re.findall(r'^([0-9a-fA-F]+)\s+([tT])\s+(\S+)\s*$',(root/'symbol.txt').read_text(),re.M))
addresses=[address for address,name in symbols];reports=[]
for players in (1,2):
 rows=list(csv.DictReader((root/f'genplus-{players}p.csv').open()));counts=collections.Counter();pcs=collections.Counter();misses=0
 for before,after in zip(rows,rows[1:]):
  if after['present']!=before['present']:continue
  misses+=1;pc=int(after['pc']);i=bisect.bisect_right(addresses,pc)-1
  name,offset=(symbols[i][1],pc-symbols[i][0]) if i>=0 else ('unmapped',pc)
  counts[name]+=1;pcs[(name,offset)]+=1
 reports.append({'players':players,'missed_samples':misses,'nearest_text_symbols':counts.most_common(20),'top_pc_offsets':[{'symbol':name,'offset':offset,'samples':n}for (name,offset),n in pcs.most_common(20)]})
report={'rom_sha256':hashlib.sha256((root/'rom.bin').read_bytes()).hexdigest(),'symbol_sha256':hashlib.sha256((root/'symbol.txt').read_bytes()).hexdigest(),'modes':reports,'scope':'PC samples only on refresh boundaries with unchanged presentation count. Nearest text labels can be internal markers and inlined functions are attributed to their parent. Sampling bias and refresh-driven trajectory changes prohibit interpreting these counts as cycle shares or causal before/after speedups.'}
(root/'missed-pc-samples.json').write_text(json.dumps(report,indent=2)+'\n')
for row in reports:print(row['players'],row['missed_samples'],row['nearest_text_symbols'][:8])
