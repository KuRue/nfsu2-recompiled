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
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn import x86_const as X

ROOT = Path(__file__).resolve().parents[1]
BASE, SIZE, STACK, STOP = 0x400000, 0x100000, 0x4f0000, 0x400100
REGISTERS = [X.UC_X86_REG_EAX, X.UC_X86_REG_ECX, X.UC_X86_REG_EDX, X.UC_X86_REG_EBX,
             X.UC_X86_REG_ESP, X.UC_X86_REG_EBP, X.UC_X86_REG_ESI, X.UC_X86_REG_EDI]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
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
        functions, roots = {}, {}
        for line in symbols.splitlines():
            fields = line.split()
            if len(fields) == 4 and fields[2].lower() == 't':
                address, length = int(fields[0], 16), int(fields[1], 16)
                functions[address] = code[address-0x401000:address-0x401000+length]
                if fields[3].startswith('root_'):
                    roots[fields[3]] = address
        emitted = generator.generate(functions, pcrecomp)
        (work / 'fixture_continuations.h').write_text(emitted)
        (work / 'fixture_bytes.h').write_text('static const unsigned char fixture_bytes[]={' + ','.join(map(str, code)) + '};\n')
        binary = work / 'generated'
        subprocess.run([os.environ.get('CXX', 'g++'), '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-isystem', str(ROOT / 'android/app/src/main/cpp'), '-isystem', str(pcrecomp / 'runtime/recomp32'),
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
            cpu.emu_start(root, STOP, count=10000)
            if cpu.reg_read(X.UC_X86_REG_EIP) != STOP:
                raise RuntimeError('x86 reference exceeded execution budget')
            reference = {'registers': [cpu.reg_read(r) for r in REGISTERS],
                         'flags': cpu.reg_read(X.UC_X86_REG_EFLAGS) & 0xcd5,
                         'memory_crc': zlib.crc32(bytes(cpu.mem_read(BASE, SIZE)))}
            def run(mode, *extra):
                return subprocess.check_output([str(binary), mode, build, str(root), *map(str, extra)], text=True)
            baseline = json.loads(run('baseline'))
            actual = {key: baseline[key] for key in reference}
            if actual != reference:
                raise RuntimeError(f'{name}: generated execution disagrees with x86: {actual} != {reference}')
            path = work / 'state.nfsproto'
            for step in range(baseline['steps']):
                run('capture', step, path)
                restored = json.loads(run('restore', path))
                if any(restored[key] != reference[key] for key in reference):
                    raise RuntimeError(f'{name}: restore at instruction {step} disagrees with x86')
                checked += 1
            print(f"{name}: x86 match and {baseline['steps']} fresh-process checkpoints passed", flush=True)
        # Fail closed rather than emit a partial implementation for unsupported code.
        for encoding in [b'\xd9\xe8\xc3', b'\x0f\x28\xc0\xc3', b'\xe8\x00\x10\x00\x00\xc3', b'\xff\x10\xc3', b'\x66\x89\xd8\xc3', b'\x64\x8b\x00\xc3']:
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
        print(f'Generated continuation proof passed: {len(roots)} programs, {checked} instruction checkpoints; actual game snapshots remain unimplemented.')

if __name__ == '__main__':
    main()
