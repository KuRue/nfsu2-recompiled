# Generated continuation proof

This opt-in experiment reuses patched pcrecomp Lifter.lift_instruction for
non-control integer operations, selected x87/MMX/SSE operations and conditional branches. Its own strict control
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
unlisted floating/vector instructions, unsupported narrow/segmented operands,
unknown direct/import calls and memory-indirect calls. Unknown register-call targets are rejected at
runtime. This is not a security boundary for arbitrary untrusted machine code.
Tests do not establish complete x86 instruction coverage.

## Remaining integration work

Actual game snapshots remain unfinished. This proof does not support complete
x87/MMX/SSE semantics, exceptions/longjmp, segmented addressing, jump tables, real
host imports, multiple threads, guest heap/protection metadata or graphics/audio/
file/network resources. The next scope is a coordinated logical thread scheduler
and resource reconstruction, alongside broader instruction coverage.
Do not run the production game through this emitter or expose a snapshot-success
button based on these tests. The installed app is unchanged.


## Extended-state and synthetic host-call milestone

The current total is14 programs and245 dispatch checkpoints. The integer-only
seven-program191-instruction table above remains the initial subset. Added cases:

| Program | Dispatch checkpoints |
| --- | ---: |
| SSE values across a nested call and packed arithmetic |8 |
| MMX packed integer values across a nested call |7 |
| Live x87 stack across a call and arithmetic |7 |
| x87 comparison state across a call |10 |
| x87 exact int64 shadow above2^53 across a call |6 |
| x87 control word and MXCSR load/store |6 |
| Two calls through the synthetic host boundary |10 |

The generated runner spills/restores the pinned runtime's eight double x87 slots,
eight int64 shadows, stack-depth counter, control word, local comparison state,
eight MMX values, eight128-bit XMM values and MXCSR. Snapshot schema2 stores these
as explicit little-endian words and preserves raw bit patterns, including signed
zero/NaN encodings at the codec level. Extended field round trips and invalid
control/depth/pending-call descriptors are tested by the codec proof. Schema1 is
rejected. No state files existed in the installed app to migrate.

XMM lanes, MXCSR and x87 control word are also compared with Unicorn. x87/MMX live
state is exercised through subsequent guest-memory outputs, including exact
9007199254740993 int64 storage. This is not proof of full architectural x87 state:
the pinned lifter uses doubles rather than80-bit extended registers, has incomplete
exception/status/tag behavior, and models MMX separately from x87 aliases. Tests
use representable arithmetic and separate FP/MMX programs. Production's optional
sliding x87 window mode is not enabled in this runner. No claim of full instruction,
rounding/exception or mixed-mode compatibility is made.

Only the listed whitelist is emitted. Unknown vector/FP operations remain errors.
The generated runner checks every continuation function/PC before dispatching a
pending host operation. It is experimental code for trusted synthetic inputs, not
an untrusted save-file sandbox.

### Host boundary protocol

A declared synthetic service has a stable logical ID and three serialized states:
idle, requested, completed-awaiting-return. A guest call pushes its real return
address and records integer inputs. Requested executes the service and stages its
result; completed applies that result and pops the checked return address. This
allows snapshots on either side of execution. The fixture calls twice, must return
74 and increment its guest-memory effect counter exactly twice; fresh restores
exercise idle, requested and completed states without duplicating that logical
counter. These extra phases explain why the total is dispatch checkpoints rather
than exclusively x86 instruction boundaries.

This service has **no external resources or side effects**: its only effect is in
serialized guest memory. This protocol alone cannot guarantee exactly-once file,
network or OS effects across a crash between an external operation and durable
snapshot publication. Real services require idempotency/recovery journals or an
explicit checkpoint restriction and reconstruction contract. Unknown imports and
resource masks are still rejected. No actual game import is resumable yet.

Optional sanitizer run:

```
build/continuation-venv/bin/python scripts/check_generated_continuations.py --sanitize
```


The extended sanitizer run initially exposed an unaligned uint64 cast in the
pinned runtime's x86-host memory macro (the appended int64 constant is deliberately
not naturally aligned). This host harness now overrides guest memory accesses with
byte-aligned alias types for16/32/64-bit integer and32/64-bit float access, matching
the runtime's existing ARM-host policy, and disables strict aliasing. It does not
change the pinned pcrecomp files or Android runtime. Sanitizer mode is fail-fast
with -fno-sanitize-recover=all so diagnostics cannot be mistaken for a passing run.
