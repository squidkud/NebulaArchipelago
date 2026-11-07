import re, struct, subprocess, bisect
dll = open(r'build/game/smg_game.dll', 'rb').read()
pe = struct.unpack_from('<I', dll, 0x3c)[0]
nsec = struct.unpack_from('<H', dll, pe + 6)[0]
opt = struct.unpack_from('<H', dll, pe + 20)[0]
secs = []
o = pe + 24 + opt
for i in range(nsec):
    vs, va, rs, rp = struct.unpack_from('<IIII', dll, o + 8 + i * 40)
    secs.append((va, vs, rp, rs))
def off(rva):
    for va, vs, rp, rs in secs:
        if va <= rva < va + max(vs, rs):
            return rp + rva - va
want = bytes.fromhex(open('tools_crash_code.hex').read())
a0 = off(0x47b4c9)
got = dll[a0:a0 + len(want)]
if got == want:
    print('CODE: IDENTICAL to the crashed build')
else:
    d = next((i for i in range(len(want)) if got[i] != want[i]), -1)
    print('CODE: DIFFERENT, first diff at', d, 'same bytes:', sum(1 for i in range(len(want)) if got[i] == want[i]), '/', len(want))
print('got :', got[0x70:0x90].hex())
print('want:', want[0x70:0x90].hex())
pubs = []
for line in open('build/game/smg_game.map', errors='replace'):
    m = re.match(r'\s*([0-9a-f]{4}):([0-9a-f]{8})\s+(\S+)\s+([0-9a-f]{16})\s', line)
    if m:
        pubs.append((int(m.group(4), 16), m.group(3)))
pubs.sort()
print('symbols:', len(pubs))
keys = [p[0] for p in pubs]
img = 0x180000000
def und(n):
    try:
        r = subprocess.run([r'C:\Program Files\LLVM\bin\llvm-undname.exe', n], capture_output=True, text=True)
        return r.stdout.strip().split('\n')[-1]
    except Exception:
        return n
for a in [0x47b549, 0x47b537, 0x3b21f1, 0x3b222e, 0x441c5b, 0x4457a5, 0x4412cd, 0x4406aa, 0x4425e9, 0x442980, 0x442918, 0x442105, 0x56b7fb, 0x5757c2, 0x574a8c, 0x573c4d, 0x5f2fd0, 0x5f3900]:
    i = bisect.bisect_right(keys, img + a) - 1
    if i >= 0:
        k, n = pubs[i]
        print(hex(a), '=', hex(img + a - k), 'bytes into', und(n)[:200])
    else:
        print(hex(a), 'no symbol')
