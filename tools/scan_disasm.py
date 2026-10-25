import subprocess, re, sys

BIN = r'D:/genesis ports/_capcom/son son/sonson_native/tools/ss_main_6809.bin'
UNIDASM = r'D:/portForge/tools/mame/unidasm.exe'

out = subprocess.run([UNIDASM, BIN, '-arch', 'm6809', '-basepc', '0'],
                     capture_output=True, text=True)
lines = out.stdout.splitlines()

# parse each line: 'ADDR: bytes   MNEM operands'
parsed = []
for ln in lines:
    s = ln.strip()
    if ':' not in s:
        continue
    addrpart = s.split(':', 1)[0]
    try:
        a = int(addrpart, 16)
    except ValueError:
        continue
    parsed.append((a, s))

mode = sys.argv[1] if len(sys.argv) > 1 else 'spriteram'

if mode == 'spriteram':
    # find any operand referencing an absolute address in 0x2020..0x20FF
    for a, s in parsed:
        m = re.search(r'\$([0-9A-Fa-f]{4})', s)
        if not m:
            continue
        v = int(m.group(1), 16)
        if 0x2020 <= v <= 0x20FF:
            print(s)
elif mode == 'range':
    lo = int(sys.argv[2], 16); hi = int(sys.argv[3], 16)
    for a, s in parsed:
        if lo <= a <= hi:
            print(s)
elif mode == 'findref':
    # find all instructions whose operand references a given address
    target = int(sys.argv[2], 16)
    for a, s in parsed:
        m = re.search(r'\$([0-9A-Fa-f]{2,4})', s)
        if not m:
            continue
        v = int(m.group(1), 16)
        if v == target:
            print(s)
