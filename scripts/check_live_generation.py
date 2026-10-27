#!/usr/bin/env python3
"""Run the experimental continuation emitter on each real function in an address list.
Calls to other functions are declared as host services; tail-jump targets are converted alongside.
Usage: check_live_generation.py ADDRESS_FILE [EXE] [--pcrecomp DIR] [--compile]
Writes nothing to the repository."""
import collections, importlib.util, json, pathlib, struct, sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86 import X86_OP_IMM
ROOT = pathlib.Path(__file__).resolve().parents[1]
args = [a for a in sys.argv[1:] if a != '--compile']
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
emitted = [];results = collections.Counter(); failures = collections.defaultdict(list)
by_address = {f['address']: f for f in catalog['functions']}
def chunks(f):
    # Merge the catalog's adjacent blocks; its 'end' field misses cold blocks placed after other functions.
    merged = []
    for start, end, *_ in sorted(f['blocks']):
        if merged and start <= merged[-1][1]:
            merged[-1][1] = max(merged[-1][1], end)
        else:
            merged.append([start, end])
    return [(start, read(start, end - start)) for start, end in merged]
def inside(f, t):
    return any(start <= t < end for start, end, *_ in f['blocks'])
def branches(f):
    for start, code in chunks(f):
        for i in decoder.disasm(code, start):
            if (i.mnemonic in ('call', 'jmp') or i.mnemonic.startswith('j')) and i.op_str.startswith('0x'):
                t = int(i.op_str, 16)
                if not inside(f, t):
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
        group[a] = chunks(by_address[a])
        pending += [t for m, t in branches(by_address[a]) if m != 'call']
    tail_targets |= group.keys() - only
    imports = {t: 1 for a in group for m, t in branches(by_address[a]) if m == 'call' and t not in group}
    try:
        emitted.append((address, g.generate(group, pcrecomp, imports)))
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
if '--compile' in sys.argv:
    # Every generated unit must also compile cleanly in the host prototype runtime.
    import os, subprocess, tempfile
    with tempfile.TemporaryDirectory(prefix='nfsu2-live-continuations-') as folder:
        source = pathlib.Path(folder) / 'live.cpp'
        parts = ['#include "cpu_test_runtime.h"',
                 'inline void generated_host_step(snapshot_prototype::State&){throw std::runtime_error("host");}']
        parts += [f'namespace live_{a:08x} {{\n{text}}}' for a, text in emitted]
        parts.append('int main(){return 0;}')
        source.write_text('\n'.join(parts))
        subprocess.run([os.environ.get('CXX', 'g++'), '-std=c++17', '-O0', '-fno-strict-aliasing', '-Wall', '-Wextra', '-Werror',
                        '-Wno-unused-function', '-isystem', str(ROOT / 'android/app/src/main/cpp'), '-isystem', str(pcrecomp / 'runtime/recomp32'),
                        '-I', str(ROOT / 'experimental/save_states'), '-c', str(source), '-o', str(source.with_suffix('.o'))], check=True)
    print(f'{len(emitted)} generated units compile with -Wall -Wextra -Werror')
