#!/usr/bin/env python3
"""Real mixed-art presenter plus production entity tail, including cached fallback."""
from pathlib import Path
import subprocess,json,runpy
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/shared-tail-render';o.mkdir(parents=True,exist_ok=True)
runpy.run_path(str(r/'tools/verify_front_sprite_chain.py'))
c=(r/'out/parity-audit/front-sprite-chain/test.c').read_text()
a=c.index('u16 arcade_large_present_mixed(');b=c.index('\n',a)
c=c[:a]+'''static u8 deny_upload;static unsigned uploads;static u16 last_frames[16];
u16 arcade_large_art_frame(u8 code,u8 attr){(void)attr;return code;}
u8 arcade_large_upload(const u16 frames[16],u16 bytes,u16 ops){assert(bytes==512 && ops==1);uploads++;memcpy(last_frames,frames,sizeof(last_frames));return !deny_upload;}
'''+c[b:]
c=c[:c.index('int main(void)')]+r'''
int main(void){unsigned owners,k;u16 sat[320],saved[32];
 g_game.numPlayers=2;player_active[0]=player_active[1]=0;
 arcade_admission.large=1;arcade_admission.children=15;arcade_admission.count=4;
 for(k=0;k<4;k++)arcade_admission.order[k]=12+k;
 arcade_large_actors[0].body.active=1;arcade_large_actors[0].x=100;arcade_large_actors[0].y=100;
 for(k=0;k<4;k++){arcade_large_actors[0].body.animation.code[k]=10+k;arcade_large_actors[0].body.animation.attr[k]=0xc0;}
 for(owners=0;owners<16;owners++){
  deny_upload=0;uploads=0;large_last_count=0;large_last_visible=0;
  for(k=0;k<4;k++){
   arcade_weapons[k].active=(owners>>k)&1;arcade_weapons[k].x=100+k*16;arcade_weapons[k].y=100;
   arcade_k_actors[k].active=1;arcade_k_actors[k].x=100+k*16;arcade_k_actors[k].y=100;arcade_k_actors[k].code=20+k;arcade_k_actors[k].attr=0xc0;
   /* Mirrored weapon records would draw duplicate cells in the old loop. */
   g_entities[12+k].active=arcade_weapons[k].active;g_entities[12+k].x=100+k*16;g_entities[12+k].y=139;g_entities[12+k].tile=600;
  }
  for(k=0;k<320;k++)sat[k]=0xbeef;
  assert(entity_render_all(sat,0)==8 && uploads==1);
  for(k=0;k<4;k++){
   assert((sat[k*4+2]&2047)==1308+k*4);
   assert((sat[(4+k)*4+2]&2047)==((owners&(1u<<k))?600:1356+k*4));
   assert(sat[(4+k)*4+3]==228+k*16);
   assert(last_frames[12+k]==((owners&(1u<<k))?65535:20+k));
  }
  for(k=0;k<8;k++)assert((sat[k*4+1]&127)==(k==7?0:k+1));
  for(k=32;k<320;k++)assert(sat[k]==0xbeef);
  memcpy(saved,sat,sizeof(saved));deny_upload=1;
  assert(entity_render_all(sat,0)==8 && !memcmp(saved,sat,sizeof(saved)));
  /* Same visibility but a changed K owner must not replay the old owner. */
  arcade_weapons[0].active^=1;
  for(k=0;k<320;k++)sat[k]=0xbeef;
  assert(entity_render_all(sat,0)==0);
  for(k=0;k<320;k++)assert(sat[k]==0xbeef);
 }
 /* Resident-only tail needs no upload and remains visible even if DMA is full. */
 arcade_admission.large=0;uploads=0;deny_upload=1;
 for(k=0;k<4;k++)arcade_weapons[k].active=1;
 assert(entity_render_all(sat,0)==4 && uploads==0);
 for(k=0;k<4;k++)assert((sat[k*4+2]&2047)==600);
 /* Capacity rejection is transactional, including resident cells. */
 {ArcadeLargeCell cells[16]={{0}};u16 resident[4]={600,600,600,600};static const ArcadeSpriteView view={0,1,256,224};
  cells[12].x=100;cells[12].y=140;cells[12].attr=0xc0;
  for(k=0;k<320;k++)sat[k]=0xbeef;
  assert(arcade_large_present_mixed(cells,4096,resident,&view,sat,2,2,512,1)==65535);
  for(k=0;k<320;k++)assert(sat[k]==0xbeef);
 }
 return 0;}
'''
(o/'test.c').write_text(c)
files=['arcade_f_render','arcade_f_art','arcade_e_render','arcade_e_art','arcade_d_render','arcade_d_art','arcade_c_render','arcade_c_art','arcade_main_render','arcade_main_art','arcade_bonus_art','arcade_cell_sat','arcade_large_render','arcade_large_present']
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),*[str(r/'src'/f'{x}.c')for x in files],'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'owner_combinations':16,'fallback_scenarios':32,'resident_only_cases':1,'capacity_cases':1,'scope':'Production entity tail and real mixed presenter: large-before-K, K slot ordering, resident/dynamic tiles, no duplicated weapon cells, same-owner fallback and changed-owner rejection. Upload/art lookup shims; not target visual comparison.'}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
