#!/usr/bin/env python3
"""Report how much of the real executable the experimental continuation emitter accepts.
Reads the local catalog and SPEED2.EXE; writes nothing. Usage: continuation_coverage.py [EXE] [--only ADDRESS_FILE]"""
import json,sys,collections,pathlib
sys.path.insert(0,'experimental/save_states')
from capstone import Cs,CS_ARCH_X86,CS_MODE_32
import importlib.util
spec=importlib.util.spec_from_file_location('g','experimental/save_states/generate_continuations.py');g=importlib.util.module_from_spec(spec);spec.loader.exec_module(g)
OK=g.INTEGER|g.EXTENDED|g.JCC|g.STRING|{'call','jmp','ret'}
only=None
if '--only' in sys.argv:
    k=sys.argv.index('--only');only={int(x,16) for x in open(sys.argv[k+1]) if x.strip()};del sys.argv[k:k+2]
c=json.load(open('work/catalog.json'))
if only is not None:c['functions']=[f for f in c['functions'] if f['address'] in only]
data=open(sys.argv[1] if len(sys.argv)>1 else str(pathlib.Path.home()/'projects/nfsu2-game/SPEED2.EXE'),'rb').read()
import struct
pe=struct.unpack_from('<I',data,0x3c)[0];n=struct.unpack_from('<H',data,pe+6)[0];opt=struct.unpack_from('<H',data,pe+20)[0];sec=pe+24+opt
secs=[struct.unpack_from('<8sIIII',data,sec+40*i) for i in range(n)]
base=c['image_base']
def read(va,size):
    rva=va-base
    for name,vs,va0,rs,ro in secs:
        if va0<=rva<va0+max(vs,rs):return data[ro+rva-va0:ro+rva-va0+size]
d=Cs(CS_ARCH_X86,CS_MODE_32)
missing=collections.Counter();fbad={};weighted=collections.Counter();full=0;ins_total=0;ins_ok=0
for f in c['functions']:
    bad=set()
    for s,e,*_ in f['blocks']:
        for i in d.disasm(read(s,e-s),s):
            ins_total+=1
            if i.mnemonic in OK:ins_ok+=1
            else:bad.add(i.mnemonic);weighted[i.mnemonic]+=1
    if not bad:full+=1
    fbad[f['address']]=bad
    for m in bad:missing[m]+=1
print('functions',len(c['functions']),'fully supported',full)
print('instructions',ins_total,'supported',ins_ok,f'{100*ins_ok/ins_total:.1f}%')
print('distinct unsupported mnemonics',len(missing))
print('top blockers (functions affected):',missing.most_common(25))
if only is not None:
    for f,b in sorted(fbad.items()):
        if b:print(f'  {f:08x}',' '.join(sorted(b)))
