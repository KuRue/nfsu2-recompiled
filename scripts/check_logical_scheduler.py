#!/usr/bin/env python3
"""Fresh-process logical thread/event/timer restore proof; no production game input."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pcrecomp',type=Path,default=ROOT.parent/'pcrecomp')
    parser.add_argument('--sanitize',action='store_true')
    args=parser.parse_args();source=args.pcrecomp.resolve()
    pin=json.loads((ROOT/'experimental/save_states/pcrecomp_source.json').read_text())
    for path,digest in pin['files'].items():
        if hashlib.sha256((source/path).read_bytes()).hexdigest()!=digest:raise RuntimeError('Pinned pcrecomp input changed')
    spec=importlib.util.spec_from_file_location('scheduler_generator',ROOT/'experimental/save_states/generate_continuations.py')
    generator=importlib.util.module_from_spec(spec);spec.loader.exec_module(generator)
    with tempfile.TemporaryDirectory(prefix='nfsu2-scheduler-proof-') as folder:
        work=Path(folder)
        subprocess.run(['as','--32',str(ROOT/'experimental/save_states/thread_fixtures.S'),'-o',str(work/'fixture.o')],check=True)
        subprocess.run(['ld','-m','elf_i386','-Ttext','0x401000','-e','waiter',str(work/'fixture.o'),'-o',str(work/'fixture.elf')],check=True)
        subprocess.run(['objcopy','-O','binary','-j','.text',str(work/'fixture.elf'),str(work/'fixture.bin')],check=True)
        code=(work/'fixture.bin').read_bytes();functions={};entries={};imports={}
        for line in subprocess.check_output(['nm','-S','--defined-only',str(work/'fixture.elf')],text=True).splitlines():
            fields=line.split()
            if len(fields)!=4 or fields[2].lower()!='t':continue
            address,length=int(fields[0],16),int(fields[1],16);name=fields[3]
            if name.startswith('import_'):
                imports[address]={'import_wait':2,'import_signal':3,'import_sleep':4}[name]
            else:
                functions[address]=code[address-0x401000:address-0x401000+length];entries[name]=address
        (work/'fixture_continuations.h').write_text(generator.generate(functions,source,imports))
        header='static const unsigned char fixture_bytes[]={'+','.join(map(str,code))+'};\n'
        header+='\n'.join(f'constexpr uint32_t entry_{name}={address}u;' for name,address in entries.items())
        (work/'fixture_bytes.h').write_text(header)
        binary=work/'scheduler';flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all'] if args.sanitize else []
        subprocess.run([os.environ.get('CXX','g++'),'-std=c++17','-O2','-fno-strict-aliasing','-Wall','-Wextra','-Werror',*flags,
                        '-isystem',str(ROOT/'android/app/src/main/cpp'),'-isystem',str(source/'runtime/recomp32'),'-I',str(work),
                        str(ROOT/'experimental/save_states/scheduler_runner.cpp'),'-o',str(binary)],check=True)
        build=hashlib.sha256(binary.read_bytes()).hexdigest()
        def run(mode,scenario,*extra):return subprocess.check_output([str(binary),mode,build,scenario,*map(str,extra)],text=True)
        print(run('selftest','timer').strip(),flush=True)
        bits=lambda n:struct.unpack('<Q',struct.pack('<d',n))[0]
        expected={
            'auto_event':(1,1,0,[0,0],[bits(1),bits(2.25)]),
            'manual_event':(2,1,1,[0,0,0],[bits(1),bits(1),bits(2.25)]),
            'timer':(1,1,0,[0],[bits(1)]),
            'periodic':(2,2,0,[0],[0]),
            'timeout':(0,0,0,[0x102],[0]),
            'sleep':(0,0,0,[42],[0])}
        total=0;seen=set()
        for scenario,values in expected.items():
            baseline=json.loads(run('baseline',scenario))
            actual=tuple(baseline[key] for key in ['counter','signals','event_set','results','fp_outputs'])
            if actual!=values:raise RuntimeError(f'{scenario} independent outcome mismatch: {actual} != {values}')
            path=work/'threads.nfsproto'
            for turn in range(baseline['turns']):
                capture=json.loads(run('capture',scenario,turn,path));seen.update(capture['statuses'])
                restored=json.loads(run('restore',scenario,path))
                if any(restored[key]!=baseline[key] for key in baseline if key!='turns'):
                    raise RuntimeError(f'{scenario} differs after restore at dispatcher turn {turn}')
                total+=1
            print(f"{scenario}: {baseline['turns']} fresh-process checkpoints, virtual time {baseline['time']}",flush=True)
        if seen!={0,1,2,3}:raise RuntimeError('Runnable/blocked/sleeping/finished captures were not all exercised')
        # A permanently blocked graph must fail cleanly, including after restoration.
        path=work/'deadlock.nfsproto';captured=json.loads(run('capture','deadlock',5,path))
        if captured['statuses']!=[1]:raise RuntimeError('Deadlock fixture was not captured blocked')
        for mode,extra in [('baseline',[]),('restore',[str(path)])]:
            failed=subprocess.run([str(binary),mode,build,'deadlock',*extra],capture_output=True,text=True)
            if failed.returncode==0 or 'Logical scheduler deadlock' not in failed.stderr:raise RuntimeError('Deadlock did not fail closed')
        print(f'Logical scheduler proof passed:6 scenarios/{total} checkpoints plus blocked deadlock restore. Actual game snapshots remain unfinished.')

if __name__=='__main__':main()
