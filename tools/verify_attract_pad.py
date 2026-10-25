#!/usr/bin/env python3
"""Exercise the production arcade-to-console input adapter for both players."""
from pathlib import Path
import subprocess
import re
root = Path(__file__).resolve().parents[1]
out = root/'out/parity-audit/attract-pad'
out.mkdir(parents=True, exist_ok=True)
source = (root/'src/game.c').read_text()
start = source.index('static ArcadeAttract attract_input;')
end = source.index('\n/* Mode chosen', start)
sdk = Path.home()/'My_apps/portForge/tools/sgdk'
joy = (sdk/'inc/joy.h').read_text()
names = ['UP', 'DOWN', 'LEFT', 'RIGHT', 'B']
defines = '\n'.join('#define BUTTON_'+name+' '+re.search(r'#define BUTTON_'+name+r'\s+(0x[0-9a-fA-F]+)', joy)[1] for name in names)
test = '#include <assert.h>\n#include "arcade_attract.h"\n'+defines+'\n'+source[start:end]+'''
int main(void){
 for(unsigned p=0;p<2;p++)for(unsigned raw=0;raw<256;raw++) {
  attract_input.input[p]=raw;attract_input.input[p^1]=raw^255;
  unsigned pad=attract_pad(p), reconstructed=0;
  if(pad&BUTTON_B)reconstructed|=1;
  if(pad&BUTTON_LEFT)reconstructed|=4;
  if(pad&BUTTON_RIGHT)reconstructed|=8;
  if(pad&BUTTON_UP)reconstructed|=16;
  if(pad&BUTTON_DOWN)reconstructed|=32;
  assert(reconstructed==(raw&61));
  assert(!(pad&~(BUTTON_B|BUTTON_LEFT|BUTTON_RIGHT|BUTTON_UP|BUTTON_DOWN)));
 }
 return 0;
}
'''
(out/'test.c').write_text(test)
subprocess.run(['cc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                '-I'+str(root/'inc'), str(out/'test.c'), '-o', str(out/'test')], check=True)
subprocess.run([str(out/'test')], check=True)
print('512 production pad mappings passed (both players, all source input bytes)')
