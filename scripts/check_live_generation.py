#!/usr/bin/env python3
"""Run the experimental continuation emitter on each real function in an address list.
Calls to other functions are declared as host services; tail-jump targets are converted alongside.
Usage: check_live_generation.py ADDRESS_FILE [EXE] [--pcrecomp DIR] [--compile]
Writes nothing to the repository."""
import collections, importlib.util, json, pathlib, struct, sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86 import X86_OP_IMM
ROOT = pathlib.Path(__file__).resolve().parents[1]
args = sys.argv[1:]
pcrecomp = ROOT.parent / 'pcrecomp'
if '--pcrecomp' in args:
    k = args.index('--pcrecomp'); pcrecomp = pathlib.Path(args[k+1]); del args[k:k+2]
spec = importlib.util.spec_from_file_location('g', ROOT / 'experimental/save_states/generate_continuations.py')
g = importlib.util.module_from_spec(spec); spec.loader.exec_module(g)
only = {int(x, 16) for x in open(args[0]) if x.strip()}
catalog = json.load(open(ROOT / 'work/catalog.json'))
data = open(args[1] if len(args) > 1 else str(pathlib.Path.home() / 'projects/nfsu2-game/SPEED2.EXE'), 'rb').read()
pe = struct.unpack_from('<I', data, 0x3c)[0]; n = struct.unpack_from('<H', data, pe+6)[0]
opt = struct.unpack_from('<H', data, pe+20)[0]; sec = pe+24+opt
secs = [struct.unpack_from('<8sIIII', data, sec+40*i) for i in range(n)]
base = catalog['image_base']
def read(va, size):
    rva = va - base
    for _, vs, va0, rs, ro in secs:
        if va0 <= rva < va0 + max(vs, rs):
            return data[ro+rva-va0:ro+rva-va0+size]
decoder = Cs(CS_ARCH_X86, CS_MODE_32)
results = collections.Counter(); failures = collections.defaultdict(list)
by_address = {f['address']: f for f in catalog['functions']}
def branches(f):
    code = read(f['address'], f['end'] - f['address'])
    for i in decoder.disasm(code, f['address']):
        if (i.mnemonic in ('call', 'jmp') or i.mnemonic.startswith('j')) and i.op_str.startswith('0x'):
            t = int(i.op_str, 16)
            if not f['address'] <= t < f['end']:
                yield i.mnemonic, t
tail_targets = set()
for address in sorted(only):
    if address not in by_address:
        continue
    # A tail jump replaces the frame, so its target must be converted too: check the tail-jump closure together.
    group, pending = {}, [address]
    while pending:
        a = pending.pop()
        if a in group or a not in by_address:
            continue
        group[a] = read(a, by_address[a]['end'] - a)
        pending += [t for m, t in branches(by_address[a]) if m != 'call']
    tail_targets |= group.keys() - only
    imports = {t: 1 for a in group for m, t in branches(by_address[a]) if m == 'call' and t not in group}
    try:
        g.generate(group, pcrecomp, imports)
        results['ok'] += 1
    except ValueError as e:
        reason = str(e).split(':')[0] if str(e).startswith('Unsupported lifted') else str(e)
        results[reason] += 1; failures[reason].append(address)
print(f'{len(tail_targets)} extra functions are reached only by tail jumps and would be converted with them')
total = sum(results.values())
print(f"{results['ok']} of {total} functions generate (with their tail-jump closure)")
for reason, count in results.most_common():
    if reason != 'ok':
        print(f'  {count:4} {reason}: ' + ' '.join(f'{a:08x}' for a in failures[reason][:6]))
