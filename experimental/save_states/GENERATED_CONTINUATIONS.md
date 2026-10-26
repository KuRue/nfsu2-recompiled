# Generated integer continuation proof

This opt-in experiment reuses patched pcrecomp Lifter.lift_instruction for
non-control integer operations and conditional branches. Its own strict control
emitter replaces native calls/returns with explicit serializable frames. It is
not enabled by the production lift script or Android build.

## Run

Use the project's prepared sibling pcrecomp checkout, Python3 with venv, GNU
binutils and C++17:

```
python3 -m venv build/continuation-venv
build/continuation-venv/bin/pip install -r experimental/save_states/requirements.txt
build/continuation-venv/bin/python scripts/check_generated_continuations.py
```

The pcrecomp source is pinned by four input hashes in pcrecomp_source.json. A new
source version must be reviewed rather than silently changing the instruction
semantics under the test. CI checks out public pcrecomp35548b8d0fd8aa853da67edcf7cd6134f6b9cdc7
and applies this repository's existing14 patches. That exactly reproduces the
tested lifter/runtime files from local patched6e85308. No pcrecomp repo push or
new production patch is needed. Capstone5.0.6 and Unicorn2.1.4 are pinned in
requirements.txt. No proprietary input, Android SDK, device or signing key is used.

## Execution model

- Each frame stores function address, next instruction PC and lazy flag locals
  (operand A/B, kind and carry). Integer registers are spilled to snapshot state
  after every instruction. DF is fixed clear; unsupported DF state is rejected.
- A direct/register-indirect call validates a known entry, pushes the *real x86
  return address* into guest memory and adds a frame. No nested C call remains.
- Return applies normal or ret-N stack cleanup and checks the popped guest return
  address against the recorded caller continuation. Callee flags are propagated
  back. Tail calls replace the current frame while retaining its guest return.
- Conditional branches use pcrecomp's existing flag expressions, with no static
  previous-instruction assumption at a checkpoint. PC/function ownership is
  checked at each step. A strict execution budget bounds the test programs.

The frame limit is64 and function-catalog limit128 for this experiment. Every
instruction is a checkpoint, favoring correctness coverage over speed. This is
not a performance-ready game backend and has not been benchmarked on a phone.

## Verification

Original fixtures.S is assembled/linked as x86-32. Unicorn executes those exact
bytes as reference. The generated program must match all eight integer registers,
the seven arithmetic/direction flag bits in mask0xcd5, and CRC32 of the entire
1MiB guest window including code and guest stack. Stack return-address writes
therefore agree with real x86 rather than dummy sentinels.

The script then captures before every executed instruction, exits that process,
and restores in a new process. Each restored result must still match the x86
reference. Seven fixtures cover:

| Program | Instruction checkpoints |
| --- | ---: |
| Direct nested calls in a loop |81 |
| Recursive calls |63 |
| Register-indirect call |13 |
| Tail call |13 |
| Carry/flags across calls and conditional branch |10 |
| ret4 callee stack cleanup |6 |
| Signed branch/overflow condition |5 |

Total:191 fresh-process checkpoints. The catalog/emitter rejects empty or
overlapping functions, fallthrough beyond a function, undecodable input,
floating/vector instructions, narrow/segmented operands, unknown direct/import
calls and memory-indirect calls. Unknown register-call targets are rejected at
runtime. This is not a security boundary for arbitrary untrusted machine code.
Tests do not establish complete x86 integer instruction coverage.

## Remaining integration work

Actual game snapshots remain unfinished. This proof does not support x87/MMX/SSE,
exceptions/longjmp, segmented addressing, jump tables, host imports, multiple
threads, guest heap/protection metadata or graphics/audio/file/network resources.
The next scope is incremental semantics coverage and resumable host-import
boundaries, followed by a coordinated logical thread scheduler/resource restore.
Do not run the production game through this emitter or expose a snapshot-success
button based on these tests. The installed app is unchanged.
