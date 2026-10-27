"""Opt-in restricted CPU continuation emitter reusing pcrecomp's instruction lifter.
Not used by run_lift.py or Android builds. All unsupported input is rejected.
"""
from pathlib import Path
import importlib.util
import re
import sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86 import X86_OP_IMM, X86_OP_REG, X86_OP_MEM

INTEGER = {'mov', 'lea', 'add', 'sub', 'xor', 'and', 'or', 'cmp', 'test', 'inc', 'dec',
           'push', 'pop', 'adc', 'sbb', 'neg', 'not', 'nop', 'imul',
           'movzx', 'movsx', 'shl', 'shr', 'sar', 'rol', 'ror', 'leave', 'cdq', 'mul', 'div', 'idiv', 'xchg',
           'sete', 'setne', 'setl', 'setle', 'setg', 'setge', 'seta', 'setae', 'setb', 'setbe', 'sets', 'setns'}
# Sub-32-bit integer operands are allowed for these; the lifter handles partial registers.
NARROW = INTEGER - {'push', 'pop', 'leave', 'cdq'}
# String operations are lifted as one atomic step, so a checkpoint never sees them half done.
STRING = {'rep stosd', 'rep stosb', 'rep movsd', 'rep movsb', 'stosd', 'stosb', 'movsd', 'movsb'}
X87 = {'fadd', 'fsub', 'fsubr', 'fmul', 'fdiv', 'fdivr', 'fsubp', 'fsubrp', 'fdivp', 'fdivrp', 'fcom', 'fcomp', 'fcompp',
       'fst', 'fabs', 'fimul', 'fiadd', 'fist', 'fsqrt'}
EXTENDED = {'fld', 'fld1', 'fldz', 'fstp', 'faddp', 'fmulp', 'fchs', 'fxch', 'fldcw', 'fnstcw', 'ftst', 'fnstsw', 'fild', 'fistp', 'movdqu', 'movups', 'movaps', 'addps', 'mulps', 'pxor', 'paddd', 'movq', 'ldmxcsr', 'stmxcsr'}
EXTENDED |= X87
JCC = {'je', 'jne', 'ja', 'jae', 'jb', 'jbe', 'jg', 'jge', 'jl', 'jle', 'js', 'jns', 'jo', 'jno', 'jp', 'jnp'}

class Instruction:
    def __init__(self, instruction):
        self.instruction = instruction
    def __getattr__(self, key):
        return getattr(self.instruction, key)
    def get_branch_target(self):
        op = self.operands[0] if self.operands else None
        return op.imm & 0xffffffff if op is not None and op.type == X86_OP_IMM else None

def generate(functions, pcrecomp, imports=None):
    imports = imports or {}
    if set(imports) & set(functions):
        raise ValueError('Synthetic import overlaps a guest function')
    if any(service not in {1,2,3,4,5,6} for service in imports.values()):
        raise ValueError("Unknown synthetic host service")
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
            if i.mnemonic not in INTEGER | EXTENDED | JCC | STRING | {'call', 'jmp', 'ret'}:
                raise ValueError(f'Unsupported instruction: {i.mnemonic}')
            if i.addr_size != 4:
                raise ValueError('Only 32-bit addressing is supported')
            for op in i.operands:
                wide = i.mnemonic in EXTENDED or i.mnemonic in STRING
                if i.mnemonic in NARROW:
                    names = module.REG_NAMES_32.keys() | module.REG_NAMES_16.keys() | module.REG_NAMES_8L.keys() | module.REG_NAMES_8H.keys()
                else:
                    names = module.REG_NAMES_32.keys()
                if not wide and op.type == X86_OP_REG and op.reg not in names:
                    raise ValueError('Only general integer registers are supported')
                if op.type == X86_OP_MEM and i.mnemonic in STRING:
                    continue  # Implicit es:[edi]/ds:[esi]; flat memory model.
                if op.type == X86_OP_MEM and (op.mem.segment or (not wide and op.size not in ((1, 2, 4) if i.mnemonic in NARROW else (4,)))):
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
    lines += ['default:return false;}}', 'inline uint32_t generated_service(uint32_t target){switch(target){']
    lines += [f'case {target}u:return {service}u;' for target,service in imports.items()]
    lines += ['default:throw std::runtime_error("Unknown declared synthetic service");}}',
              'inline void generated_step(snapshot_prototype::State& state){',
              'using snapshot_prototype::require;',
              'require(!state.frames.empty(),"No continuation");',
              'require(!(state.flags&0x400u),"Direction flag state is unsupported");',
              'auto frame=state.frames.back();',
              'std::memcpy(g_st,state.fp.data(),64);std::memcpy(g_st_i64,state.fpInt.data(),64);std::memcpy(g_mm,state.mm.data(),64);std::memcpy(g_xmm,state.xmm.data(),128);',
              'g_fp_top=int(state.fpTop);g_fpu_cw=uint16_t(state.fpControl);g_mxcsr=state.mxcsr;',
              'require(generated_owner(frame.pc)==frame.function,"PC/function mismatch");',
              'if(state.host[0]){generated_host_step(state);return;}',
              'g_eax=state.registers[0];g_ecx=state.registers[1];g_edx=state.registers[2];g_ebx=state.registers[3];',
              'g_esp=state.registers[4];g_ebp=state.registers[5];g_esi=state.registers[6];g_edi=state.registers[7];',
              'uint32_t _flag_a=frame.locals[0],_flag_b=frame.locals[1],_flag_k=frame.locals[2],_cf=frame.locals[3];',
              '[[maybe_unused]] int _df=1,_fpu_cmp=int32_t(frame.locals[4]);',
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
                if immediate is not None and immediate not in decoded and immediate not in imports:
                    raise ValueError('Unlifted/import call is unsupported')
                if immediate in imports:
                    lines += [f'target={immediate}u;', 'esp-=4;MEM32(esp)=next;action=3;']
                    lines.append('break;}')
                    continue
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
                    # The pinned lifter emits some raw floating loads/stores.
                    # Preserve x86 unaligned access in this experimental host backend.
                    # The pinned lifter builds the idiv dividend with a signed left shift, which is
                    # undefined in C++17. Build the same bits unsigned.
                    line = line.replace('((int64_t)(int32_t)edx << 32)', '(int64_t)((uint64_t)edx << 32)')
                    line = line.replace('*(double*)ADDR(', '*(volatile proto_f64*)ADDR(')
                    line = line.replace('*(float*)ADDR(', '*(volatile proto_f32*)ADDR(')
                    line = line.replace('*(float*)&MEM32(', '*(volatile proto_f32*)&MEM32(')
                    lines.append(line)
            lines.append('break;}')
    lines += ['default:throw std::runtime_error("Unknown continuation");}',
              'frame.pc=next;frame.locals={_flag_a,_flag_b,_flag_k,_cf,uint32_t(_fpu_cmp)};',
              'state.frames.back()=frame;',
              'state.registers={g_eax,g_ecx,g_edx,g_ebx,g_esp,g_ebp,g_esi,g_edi};',
              'state.flags=recomp_eflags(_flag_k,_flag_a,_flag_b,_cf,_df);',
              'std::memcpy(state.fp.data(),g_st,64);std::memcpy(state.fpInt.data(),g_st_i64,64);std::memcpy(state.mm.data(),g_mm,64);std::memcpy(state.xmm.data(),g_xmm,128);',
              'state.fpTop=uint32_t(g_fp_top);state.fpControl=g_fpu_cw;state.mxcsr=g_mxcsr;',
              'if(action==3)state.host={1,generated_service(target),g_eax,g_ecx,0};',
              'if(action==1)state.frames.push_back({target,target,frame.locals});',
              'if(action==2){state.frames.pop_back();',
              'if(state.frames.empty())require(target==0x400100u,"Invalid root return");',
              'else{require(target==state.frames.back().pc,"Guest return disagrees with continuation");state.frames.back().locals=frame.locals;}}',
              '}']
    return '\n'.join(lines) + '\n'
