#!/usr/bin/env python3
"""Verify linked G/H pose pixels and animation graph fields against source data."""
from pathlib import Path
import hashlib,json,re,struct,csv
from arcade_foe_data import extract
import gen_assets as G
r=Path(__file__).resolve().parents[1];d=extract(r/'arcade');rom=(r/'out/rom.bin').read_bytes()
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(r/'out/symbol.txt').read_text(),re.M)}
h=(r/'inc/ss_anim.h').read_text();base=int(re.search(r'#define SS_ANIM_TILE_BASE\s+(\d+)',h)[1]);count=int(re.search(r'#define SS_ANIM_TILE_COUNT\s+(\d+)',h)[1]);g=G.Roms(str(r/'arcade'));checks=0
for k,f in enumerate(d['frames']):
 a=syms['arcade_foe_frames']+k*8;next_frame,tile,code,attr,duration=struct.unpack('>HHBBB',rom[a:a+7]);assert(next_frame,code,attr,duration)==(f['next'],f['code'],f['attr'],f['duration'])
 assert base<=tile and tile+4<=base+count
 actual=rom[syms['ss_anim_tiles']+(tile-base)*32:syms['ss_anim_tiles']+(tile-base)*32+128]
 source=G.bake_sprite_tiles(g,code+((attr&32)<<3),attr&31)
 assert actual==b''.join(struct.pack('>I',v)for v in source),(k,f)
 assert attr&128,'Renderer needs vertical flip for this source pose'
 checks+=128+7
world=(r/'inc/ss_level.h').read_text();world_base=int(re.search(r'#define SS_LEVEL_TILE_BASE\s+(\d+)',world)[1]);world_top=int(re.search(r'#define SS_LEVEL_TILE_TOP\s+(\d+)',world)[1]);assert base+count<=world_base<world_top<=1376
observed={(int(row['stage']),int(row['col'])):bytes.fromhex(row['collision'])for row in csv.DictReader((r/'out/parity-audit/world-oracle.csv').open())}
program=b''.join((r/'arcade'/n).read_bytes()for n in ['ss.01e','ss.02e','ss.03e'])
flat=[]
for stage in range(26):
 duration=program[0x6485+stage*7-0x4000]
 for x in range(duration*32):
  column=observed[stage,x%32];mask=0
  for tier in range(6):
   row=4+tier*4
   if column[row>>3]&(128>>(row&7)):mask|=1<<tier
  flat.append(mask)
assert len(flat)==2784 and rom[syms['ss_ribbon_collision']:syms['ss_ribbon_collision']+2784]==bytes(flat)
result={'rom_sha256':hashlib.sha256(rom).hexdigest(),'animation_frames':len(d['frames']),'pose_pairs':len(d['poses']),'collision_columns':len(flat),'bytes_compared':checks+len(flat),'mismatches':0,'sprite_tiles':[base,base+count],'background_tiles':[world_base,world_top],'source':'Original source animation graph and independently baked owner-provided graphics ROMs; linked cartridge bytes'}
(r/'out/parity-audit/foe-assets-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
