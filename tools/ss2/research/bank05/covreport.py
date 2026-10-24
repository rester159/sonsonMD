"""covreport.py OUT.cov ... -- summarise trace.py coverage: object handlers
seen in slots (bank 5 / fixed bank 3 only) and executed bank 5 routine
entry points of interest."""
import pickle, sys

ENTRIES5 = [0xC000, 0xC00E, 0xC01C, 0xC02A, 0xC048, 0xC0A0, 0xC164, 0xC1C0, 0xC222, 0xC294,
            0xC32D, 0xC3BC, 0xC408, 0xC40C, 0xC422, 0xC4C8, 0xC50B, 0xC579, 0xC5C7, 0xC5CB,
            0xC5E1, 0xC68E, 0xC70C, 0xC810, 0xC8CF, 0xC8DE, 0xC909, 0xC913, 0xC92D, 0xC9B0,
            0xCAAE, 0xCB1C, 0xCBB4, 0xCBC6, 0xCBF8, 0xCC0A, 0xCC91, 0xCCBD, 0xCD5F, 0xCE20,
            0xCEED, 0xCEFC, 0xCF5B, 0xCFE9, 0xD064, 0xD8B2, 0xD92F, 0xD967]
ENTRIES3 = [0x8AD8, 0x8BD7, 0x8BF3, 0x8C0F, 0x8C2F, 0x8C66, 0x8CAB, 0x8D67, 0x8DAA, 0x8DEF,
            0x8E03, 0x8E2A, 0x93DA, 0x9516, 0x9591]
tot_h, cov = {}, {}
for f in sys.argv[1:]:
    d = pickle.load(open(f, 'rb'))
    for k, v in d['handlers'].items():
        tot_h[k] = tot_h.get(k, 0) + v
    for k, v in d['cov'].items():
        cov[k] = cov.get(k, 0) + v
print('handlers (bank, addr, kind): frames')
for (b, h, kind), v in sorted(tot_h.items()):
    if b == 5 or (b == 0 and 0x8000 <= h < 0xA000):
        print('  %02X $%04X %-3s %d' % (b, h, kind, v))
print('bank 5 entries executed:', ' '.join('$%04X:%d' % (a, cov.get((5, a), 0)) for a in ENTRIES5))
print('bank 3 entries executed:', ' '.join('$%04X:%d' % (a, cov.get((3, a), 0)) for a in ENTRIES3))
