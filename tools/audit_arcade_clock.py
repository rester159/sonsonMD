#!/usr/bin/env python3
"""ROM-grounded clock ownership audit. Reports current native release blockers.
Static evidence only: does not launch an emulator or assert full IRQ execution.
"""
from pathlib import Path
import hashlib,json,re
r=Path(__file__).resolve().parents[1]
o=r/'out/parity-audit/clock-ownership';o.mkdir(parents=True,exist_ok=True)
rom=b''.join((r/'arcade'/n).read_bytes() for n in ('ss.01e','ss.02e','ss.03e'))
sha=hashlib.sha256(rom).hexdigest()
assert sha=='09243a5a27a14d1e31db7456e1795016cd4da6466dc7735a1852eb102d8400d1'
def at(pc,n):
 assert 0x4000<=pc<=0x10000-n
 return rom[pc-0x4000:pc-0x4000+n]
def guard(pc,hexbytes):
 expected=bytes.fromhex(hexbytes);assert at(pc,len(expected))==expected,hex(pc)
guard(0xfff8,'910d')
guard(0x9112,'cc0001d323dd23')
guard(0x9130,'963b85802608d602588e9141ad953b')
guard(0x9636,'9605488e963e6e96')
guard(0x9646,'d608588e964e6e95')
guard(0x979d,'d60b588e97a56e95')
guard(0x97ab,'bd9a2cbda5e2bde2b3bdef51bdfed8')
# Foreground command dispatcher selects the bulk-loader handler. Its IRQ
# unmask is real code; linear-disassembly ANDCC matches in map data are not.
guard(0x41b0,'8e41bead969e12ec844824e320f7')
guard(0x41d4,'54b0')
guard(0x54ca,'1a10ec81eda1ec81eda1ec81eda1ec81eda1ec81eda11cef0af426e439')
guard(0x412d,'b6300643973b39')

def table(pc,n):return [int.from_bytes(at(pc+2*k,2),'big') for k in range(n)]
assert table(0x9141,3)==[0x9258,0x9296,0x9636]
assert table(0x963e,4)==[0xa2c5,0x9646,0x9bd2,0x9d60]
assert table(0x964e,5)==[0x9658,0x96a6,0x979d,0x97e3,0x97f9]
assert table(0x97a5,3)==[0x97ab,0x97c6,0x97d2]
# Decode the guarded arithmetic instructions, independently of native clocks.
def source_increment(word):
 pc=0x9112;d=None;memory={0x23:word>>8,0x24:word&255}
 while pc<0x9119:
  op=at(pc,1)[0]
  if op==0xcc:d=int.from_bytes(at(pc+1,2),'big');pc+=3
  elif op==0xd3:
   address=at(pc+1,1)[0];d=(d+(memory[address]<<8|memory[address+1]))&65535;pc+=2
  elif op==0xdd:
   address=at(pc+1,1)[0];memory[address],memory[address+1]=d>>8,d&255;pc+=2
  else:raise AssertionError(hex(op))
 return memory[0x23]<<8|memory[0x24]
for word in range(65536):assert source_increment(word)==(word+1)&65535
entity=(r/'src/entity.c').read_text();game=(r/'src/game.c').read_text()
high=re.search(r'arcade_main_pool_schedule\(&arcade_main,arcade_main_ready,(.*?),wave_clock,',entity)
assert high, 'Clock API changed: reassess this audit against current code'
assert high.group(1)=='wave_clock_high'
assert 'static u8 wave_clock, wave_clock_high;' in entity and 'wave_clock=0;' not in entity
assert 'if(++wave_clock==0)++wave_clock_high;' in entity and 'vtimer' not in entity
assert game.index('if (g_paused)',game.index('void game_run('))<game.index('done = game_update();',game.index('void game_run('))
# Best-case initially aligned clocks. An extra delivered IRQ without a simulation advance breaks
# coherence; report this as a known failing release requirement, not a pass.
examples=[]
for source_before,extra_irqs in [(0xfe,1),(0xff,1),(0xfffe,1),(0xffff,1),(0x1234,7)]:
 source_after=source_before
 for _ in range(1+extra_irqs):source_after=source_increment(source_after)
 native_low=(source_before+1)&255
 mixed=(source_after&0xff00)|native_low
 assert mixed!=source_after
 examples.append({'initial_word':source_before,'delivered_irqs':1+extra_irqs,'source_word':source_after,'native_mixed_word':mixed})
report={'source_sha256':sha,'arithmetic_states':65536,
 'evidence_kind':'Guarded ROM instructions and dispatch tables; narrow arithmetic interpreter, not whole CPU or board execution',
 'gameplay_dispatch':{'mode_02':2,'state_05':1,'state_08':2,'state_0b':0,'entry':'97AB','calls':['9A2C','A5E2','E2B3','EF51','FED8']},
 'ordering':'IRQ increments23:24 before3B-bit7 gate and mode02 dispatch; gameplay A5E2 and scheduler EF51 share the IRQ-owned counter.',
 'native_coherent_byte_owner':'PASS (structural; arithmetic checked by verify_main_refresh_byte)', 'full_source_clock_gate':'FAIL: cadence and lifecycle integration pending','historical_mixed_clock_counterexamples':examples,
 'implementation_requirements':['One16-bit source clock and one explicit ownership policy; both formation bytes come from that clock.','Account for boot/menu/game/results/course lifecycle without resetting only one byte.','Preserve Home pause as an intentional mode extension; do not let elapsed paused refreshes jump gameplay phases.','Verify original IRQ nesting/unmasking and original refresh cadence before claiming exact timing.','Repeat source/native trajectories and performance tests after timing changes.'],
 'additional_rom_evidence':{'freeze':'DSW2 read at412D is inverted into3B; bit7 gate9130 corresponds to Freeze, not Service.','foreground_loader':'41B3 dispatches through41BE; entry41D4 selects54B0. Loader masks/unmasks IRQ around each10-byte chunk. This does not establish nested IRQ in normal gameplay.'},
 'reference_rate_hz':55.4,
 'reference_url':'https://github.com/mamedev/mame/blob/mame0288/src/mame/capcom/sonson.cpp',
 'source_files':{p:hashlib.sha256((r/p).read_bytes()).hexdigest()for p in ['src/entity.c','src/game.c']}}
(o/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
