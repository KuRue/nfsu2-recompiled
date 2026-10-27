#!/usr/bin/env python3
"""Generate integer resumable x86 fixtures with pcrecomp; compare with Unicorn."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import zlib
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn import x86_const as X

ROOT = Path(__file__).resolve().parents[1]
BASE, SIZE, STACK, STOP = 0x400000, 0x100000, 0x4f0000, 0x400100
REGISTERS = [X.UC_X86_REG_EAX, X.UC_X86_REG_ECX, X.UC_X86_REG_EDX, X.UC_X86_REG_EBX,
             X.UC_X86_REG_ESP, X.UC_X86_REG_EBP, X.UC_X86_REG_ESI, X.UC_X86_REG_EDI]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitize', action='store_true', help='Enable AddressSanitizer and UndefinedBehaviorSanitizer')
    parser.add_argument('--pcrecomp', type=Path, default=ROOT.parent / 'pcrecomp')
    args = parser.parse_args()
    pcrecomp = args.pcrecomp.resolve()
    provenance = json.loads((ROOT / 'experimental/save_states/pcrecomp_source.json').read_text())
    for relative, digest in provenance['files'].items():
        if hashlib.sha256((pcrecomp / relative).read_bytes()).hexdigest() != digest:
            raise RuntimeError(f'pcrecomp input changed: {relative}; review and update the pinned source manifest')
    spec = importlib.util.spec_from_file_location('continuation_generator', ROOT / 'experimental/save_states/generate_continuations.py')
    generator = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(generator)
    with tempfile.TemporaryDirectory(prefix='nfsu2-generated-continuations-') as folder:
        work = Path(folder)
        subprocess.run(['as', '--32', str(ROOT / 'experimental/save_states/fixtures.S'), '-o', str(work / 'fixture.o')], check=True)
        subprocess.run(['ld', '-m', 'elf_i386', '-Ttext', '0x401000', '-e', 'root_loop', str(work / 'fixture.o'), '-o', str(work / 'fixture.elf')], check=True)
        subprocess.run(['objcopy', '-O', 'binary', '-j', '.text', str(work / 'fixture.elf'), str(work / 'fixture.bin')], check=True)
        code = (work / 'fixture.bin').read_bytes()
        symbols = subprocess.check_output(['nm', '-S', '--defined-only', str(work / 'fixture.elf')], text=True)
        functions, roots, imports = {}, {}, {}
        for line in symbols.splitlines():
            fields = line.split()
            if len(fields) == 4 and fields[2].lower() == 't':
                address, length = int(fields[0], 16), int(fields[1], 16)
                if fields[3].startswith('import_'):
                    imports[address] = 1
                    continue
                if fields[3].startswith('data_'):
                    continue
                functions[address] = code[address-0x401000:address-0x401000+length]
                if fields[3].startswith('root_'):
                    roots[fields[3]] = address
        emitted = generator.generate(functions, pcrecomp, imports)
        (work / 'fixture_continuations.h').write_text(emitted)
        (work / 'fixture_bytes.h').write_text('static const unsigned char fixture_bytes[]={' + ','.join(map(str, code)) + '};\n')
        binary = work / 'generated'
        flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-sanitize-recover=all'] if args.sanitize else []
        subprocess.run([os.environ.get('CXX', 'g++'), '-std=c++17', '-O2', '-fno-strict-aliasing', '-Wall', '-Wextra', '-Werror',
                        *flags, '-isystem', str(ROOT / 'android/app/src/main/cpp'), '-isystem', str(pcrecomp / 'runtime/recomp32'),
                        '-I', str(work), str(ROOT / 'experimental/save_states/generated_runner.cpp'), '-o', str(binary)], check=True)
        build = hashlib.sha256(binary.read_bytes()).hexdigest()
        checked = 0
        for name, root in sorted(roots.items()):
            cpu = Uc(UC_ARCH_X86, UC_MODE_32)
            cpu.mem_map(BASE, SIZE)
            cpu.mem_write(0x401000, code)
            cpu.mem_write(STACK, STOP.to_bytes(4, 'little'))
            cpu.reg_write(X.UC_X86_REG_ESP, STACK)
            cpu.reg_write(X.UC_X86_REG_EFLAGS, 0x202)
            cpu.reg_write(X.UC_X86_REG_FPCW, 0x27f)
            cpu.reg_write(X.UC_X86_REG_MXCSR, 0x1f80)
            def host_call(cpu, pc, size, user):
                if pc not in imports:
                    return
                result=(cpu.reg_read(X.UC_X86_REG_EAX)+3*cpu.reg_read(X.UC_X86_REG_ECX))&0xffffffff
                count=int.from_bytes(cpu.mem_read(0x480100,4),'little')
                cpu.mem_write(0x480100,((count+1)&0xffffffff).to_bytes(4,'little'))
                stack=cpu.reg_read(X.UC_X86_REG_ESP)
                target=int.from_bytes(cpu.mem_read(stack,4),'little')
                cpu.reg_write(X.UC_X86_REG_EAX,result)
                cpu.reg_write(X.UC_X86_REG_ESP,stack+4)
                cpu.reg_write(X.UC_X86_REG_EIP,target)
            cpu.hook_add(UC_HOOK_CODE,host_call)
            cpu.emu_start(root, STOP, count=10000)
            if cpu.reg_read(X.UC_X86_REG_EIP) != STOP:
                raise RuntimeError('x86 reference exceeded execution budget')
            reference = {'registers': [cpu.reg_read(r) for r in REGISTERS],
                         'flags': cpu.reg_read(X.UC_X86_REG_EFLAGS) & 0xcd5,
                         'memory_crc': zlib.crc32(bytes(cpu.mem_read(BASE, SIZE))),
                         'xmm': [(cpu.reg_read(getattr(X, f'UC_X86_REG_XMM{i}')) >> (32*j)) & 0xffffffff for i in range(8) for j in range(4)],
                         'fp_control': cpu.reg_read(X.UC_X86_REG_FPCW), 'mxcsr': cpu.reg_read(X.UC_X86_REG_MXCSR)}
            if name == 'root_host' and (reference['registers'][0] != 74 or int.from_bytes(cpu.mem_read(0x480100,4),'little') != 2):
                raise RuntimeError('Synthetic host reference contract failed')
            def run(mode, *extra):
                return subprocess.check_output([str(binary), mode, build, str(root), *map(str, extra)], text=True)
            baseline = json.loads(run('baseline'))
            actual = {key: baseline[key] for key in reference}
            if actual != reference:
                raise RuntimeError(f'{name}: generated execution disagrees with x86: {actual} != {reference}')
            path = work / 'state.nfsproto'
            phases = set()
            for step in range(baseline['steps']):
                captured = json.loads(run('capture', step, path))
                phases.add(captured['host_phase'])
                restored = json.loads(run('restore', path))
                if any(restored[key] != reference[key] for key in reference):
                    raise RuntimeError(f'{name}: restore at instruction {step} disagrees with x86')
                checked += 1
            if name == 'root_host' and phases != {0, 1, 2}:
                raise RuntimeError('Pending/completed host-call snapshots were not exercised')
            print(f"{name}: x86 match and {baseline['steps']} fresh-process execution checkpoints passed", flush=True)
        # Fail closed rather than emit a partial implementation for unsupported code.
        for encoding in [b'\xd9\xfe\xc3', b'\x0f\x5e\xc0\xc3', b'\xe8\x00\x10\x00\x00\xc3', b'\xff\x10\xc3', b'\x0f\xa2\xc3', b'\x0f\xb6\xc4\x0f\xa2\xc3', b'\x64\x8b\x00\xc3']:
            try:
                generator.generate({0x401000: encoding}, pcrecomp)
            except ValueError:
                pass
            else:
                raise RuntimeError('Unsupported instruction/import/control flow was accepted')
        for functions in [{}, {0x401000: b''}, {0x401000: b'\x90'},
                          {0x401000: b'\xc3\xc3', 0x401001: b'\xc3'}]:
            try:
                generator.generate(functions, pcrecomp)
            except ValueError:
                pass
            else:
                raise RuntimeError('Malformed function catalog was accepted')
        print(f'Generated continuation proof passed: {len(roots)} programs, {checked} dispatch checkpoints; actual game snapshots remain unimplemented.')

if __name__ == '__main__':
    main()
