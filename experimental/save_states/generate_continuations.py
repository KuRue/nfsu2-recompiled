"""Opt-in integer-only continuation emitter reusing pcrecomp's instruction lifter.
Not used by run_lift.py or Android builds. All unsupported input is rejected.
"""
from pathlib import Path
import importlib.util
import re
import sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86 import X86_OP_IMM, X86_OP_REG, X86_OP_MEM

INTEGER = {'mov', 'lea', 'add', 'sub', 'xor', 'and', 'or', 'cmp', 'test', 'inc', 'dec',
           'push', 'pop', 'adc', 'sbb', 'neg', 'not', 'nop', 'imul'}
JCC = {'je', 'jne', 'ja', 'jae', 'jb', 'jbe', 'jg', 'jge', 'jl', 'jle', 'js', 'jns', 'jo', 'jno', 'jp', 'jnp'}

class Instruction:
    def __init__(self, instruction):
        self.instruction = instruction
    def __getattr__(self, key):
        return getattr(self.instruction, key)
    def get_branch_target(self):
        op = self.operands[0] if self.operands else None
        return op.imm & 0xffffffff if op is not None and op.type == X86_OP_IMM else None

def generate(functions, pcrecomp):
    if not functions or len(functions) > 128:
        raise ValueError('Invalid function count for experimental emitter')
    spans = sorted((address, address + len(code)) for address, code in functions.items())
    if any(start < 4096 or end <= start or end > 0x100000000 for start, end in spans):
        raise ValueError('Invalid function range')
    if any(left[1] > right[0] for left, right in zip(spans, spans[1:])):
        raise ValueError('Overlapping function ranges')
    path = Path(pcrecomp) / 'tools/lift/lift32.py'
    spec = importlib.util.spec_from_file_location('snapshot_lift32', path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    decoder = Cs(CS_ARCH_X86, CS_MODE_32)
    decoder.detail = True
    decoded = {}
    owner = {}
    for address, code in functions.items():
        instructions = [Instruction(i) for i in decoder.disasm(code, address)]
        if sum(i.size for i in instructions) != len(code):
            raise ValueError('Undecodable function bytes')
        for i in instructions:
            if i.address in owner:
                raise ValueError('Overlapping functions')
            owner[i.address] = address
            if i.mnemonic not in INTEGER | JCC | {'call', 'jmp', 'ret'}:
                raise ValueError(f'Unsupported instruction: {i.mnemonic}')
            if i.addr_size != 4:
                raise ValueError('Only 32-bit addressing is supported')
            for op in i.operands:
                if op.type == X86_OP_REG and op.reg not in module.REG_NAMES_32:
                    raise ValueError('Only 32-bit integer registers are supported')
                if op.type == X86_OP_MEM and (op.size != 4 or op.mem.segment):
                    raise ValueError('Only unsegmented 32-bit memory operands are supported')
        decoded[address] = instructions
    def target(insn, lifter):
        op = insn.operands[0]
        if op.type == X86_OP_IMM:
            return f'0x{op.imm & 0xffffffff:08x}u'
        if op.type == X86_OP_REG:
            return lifter._fmt_read(op)
        raise ValueError('Memory-indirect control flow is not implemented')
    lines = ['// Generated ONLY from synthetic test input; no production code.',
             'inline uint32_t generated_owner(uint32_t pc){switch(pc){']
    for pc, function in sorted(owner.items()):
        lines.append(f'case 0x{pc:x}u:return 0x{function:x}u;')
    lines += ['default:throw std::runtime_error("Unknown generated continuation PC");}}',
              'inline bool generated_entry(uint32_t pc){switch(pc){']
    lines += [f'case 0x{address:x}u:return true;' for address in functions]
    lines += ['default:return false;}}',
              'inline void generated_step(snapshot_prototype::State& state){',
              'using snapshot_prototype::require;',
              'require(!state.frames.empty(),"No continuation");',
              'require(!(state.flags&0x400u),"Direction flag state is unsupported");',
              'auto frame=state.frames.back();',
              'require(generated_owner(frame.pc)==frame.function,"PC/function mismatch");',
              'g_eax=state.registers[0];g_ecx=state.registers[1];g_edx=state.registers[2];g_ebx=state.registers[3];',
              'g_esp=state.registers[4];g_ebp=state.registers[5];g_esi=state.registers[6];g_edi=state.registers[7];',
              'uint32_t _flag_a=frame.locals[0],_flag_b=frame.locals[1],_flag_k=frame.locals[2],_cf=frame.locals[3];',
              '[[maybe_unused]] int _df=1,_fpu_cmp=0;',
              'uint32_t next=0,target=0,action=0;switch(frame.pc){']
    for function, instructions in decoded.items():
        lifter = module.Lifter(precise_carry=True)
        lifter._labels = {i.address for i in instructions}
        lifter._jump_targets = set()
        for i in instructions:
            following = i.address + i.size
            lines.append(f'case 0x{i.address:x}u:{{next=0x{following:x}u; // {i.mnemonic} {i.op_str}')
            if i.mnemonic == 'call':
                immediate = i.get_branch_target()
                if immediate is not None and immediate not in decoded:
                    raise ValueError('Unlifted/import call is unsupported')
                lines += [f'target={target(i,lifter)};',
                          'require(generated_entry(target),"Unknown call target");',
                          'require(state.frames.size()<64,"Continuation stack limit");',
                          'esp-=4;MEM32(esp)=next;action=1;']
            elif i.mnemonic == 'ret':
                cleanup = i.operands[0].imm if i.operands else 0
                lines += [f'target=MEM32(esp);esp+={4+cleanup}u;action=2;']
            elif i.mnemonic == 'jmp':
                immediate = i.get_branch_target()
                if immediate in lifter._labels:
                    lines.append(f'next=0x{immediate:x}u;')
                else:
                    if immediate is not None and immediate not in decoded:
                        raise ValueError('Jump outside known functions')
                    lines += [f'target={target(i,lifter)};',
                              'require(generated_entry(target),"Unknown tail-call target");frame.function=target;next=target;']
            else:
                if following not in lifter._labels:
                    raise ValueError('Function can fall through its boundary')
                if i.mnemonic in JCC and i.get_branch_target() not in lifter._labels:
                    raise ValueError('Conditional jump outside function')
                lifter._flag_state = None  # Every instruction is a possible fresh-process checkpoint.
                emitted = lifter.lift_instruction(i)
                for line in emitted:
                    if any(token in line for token in ('RECOMP_TODO','RECOMP_UNIMPL','RECOMP_CALL','RECOMP_ICALL','RECOMP_ITAIL')):
                        raise ValueError(f'Unsupported lifted emission: {line}')
                    line = re.sub(r'goto L_([0-9A-Fa-f]+);', lambda m: f'next=0x{m[1]}u;', line)
                    if 'goto ' in line:
                        raise ValueError('Unexpected emitted control flow')
                    lines.append(line)
            lines.append('break;}')
    lines += ['default:throw std::runtime_error("Unknown continuation");}',
              'frame.pc=next;frame.locals={_flag_a,_flag_b,_flag_k,_cf};',
              'state.frames.back()=frame;',
              'state.registers={g_eax,g_ecx,g_edx,g_ebx,g_esp,g_ebp,g_esi,g_edi};',
              'state.flags=recomp_eflags(_flag_k,_flag_a,_flag_b,_cf,_df);',
              'if(action==1)state.frames.push_back({target,target,frame.locals});',
              'if(action==2){state.frames.pop_back();',
              'if(state.frames.empty())require(target==0x400100u,"Invalid root return");',
              'else{require(target==state.frames.back().pc,"Guest return disagrees with continuation");state.frames.back().locals=frame.locals;}}',
              '}']
    return '\n'.join(lines) + '\n'
