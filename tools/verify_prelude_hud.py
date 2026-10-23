#!/usr/bin/env python3
"""Live title HUD fields: all credit words, high-score formatting and mode labels."""
from pathlib import Path
import json,subprocess
r=Path(__file__).resolve().parents[1];o=r/'out/parity-audit/prelude-hud';o.mkdir(parents=True,exist_ok=True)
(o/'test.c').write_text('''#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "arcade_prelude_hud.h"
#include "arcade_prelude_hud_art.h"
static unsigned glyph(unsigned bank,char c){unsigned code=c==' '?16:c>='0'&&c<='9'?c-'0':c-'A'+17;return ARCADE_PRELUDE_HUD_BASE+arcade_prelude_hud_glyphs[bank][code];}
int main(void){u16 score[8],credit[32],prompt[32];char digits[20],line[33];
 for(unsigned v=0;v<65536;v++){
  arcade_prelude_hud(v,v,0,0,score,credit,prompt);
  snprintf(digits,sizeof(digits),"%8u",v);
  for(unsigned i=0;i<8;i++)assert(score[i]==glyph(0,digits[i]));
  memset(line,' ',32);line[32]=0;memcpy(line+2,"CREDIT",6);
  snprintf(digits,sizeof(digits),"%u",v);memcpy(line+9,digits,strlen(digits));
  for(unsigned i=0;i<32;i++)assert(credit[i]==glyph(1,line[i]));
 }
 const u32 values[]={99999,100000,999999,1000000,99999999,100000000,4294967295u};
 for(unsigned v=0;v<7;v++)for(unsigned freeplay=0;freeplay<2;freeplay++)for(unsigned start=0;start<2;start++){
  arcade_prelude_hud(values[v],65535,freeplay,start,score,credit,prompt);
  snprintf(digits,sizeof(digits),"%8u",(unsigned)(values[v]%100000000));
  for(unsigned i=0;i<8;i++)assert(score[i]==glyph(0,digits[i]));
  memset(line,' ',32);line[32]=0;const char *label=start?"PRESS START BUTTON":"PLEASE DEPOSIT COIN";
  memcpy(line+7,label,strlen(label));
  for(unsigned i=0;i<32;i++)assert(prompt[i]==glyph(start?3:4,line[i]));
  if(freeplay){memset(line,' ',32);memcpy(line+2,"FREE PLAY",9);for(unsigned i=0;i<32;i++)assert(credit[i]==glyph(2,line[i]));}
 }
 return 0;}
''')
subprocess.run(['cc','-std=c99','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'inc'),str(o/'test.c'),str(r/'src/arcade_prelude_hud.c'),str(r/'src/arcade_prelude_hud_art.c'),'-o',str(o/'test')],check=True)
subprocess.run([str(o/'test')],check=True)
report={'passed':True,'credit_words':65536,'high_score_mode_boundaries':28,'scope':'Compiled HUD tile fields against independent decimal formatting: all credit words, score leading blanks/eight-digit wrap, free-play and explicit lifecycle prompt choice. Sanitizers; visual target captures separate. No production title binding.'}
(o/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
