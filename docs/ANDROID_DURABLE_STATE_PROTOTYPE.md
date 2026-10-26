# Durable state prototype and runtime integration boundary

## What works now

The standalone experimental/save_states prototype writes an explicit nested-call
continuation, exits, and resumes in a fresh process with the same output as an
uninterrupted run. Run scripts/check_snapshot_prototype.py; the GitHub source
workflow runs it too. See the experimental README for fields, limits and test
coverage. This is a successful integer-only architecture proof. It is NOT a
snapshot of NFSU2 and is not installed in the Android app.

## Why the game cannot use a memory dump

pcrecomp/runtime/recomp32/recomp_types.h RECOMP_CALL pushes RECOMP_RETADDR then
invokes the callee as a C function. Its return continuation is in the native stack,
not recoverable from that dummy guest return address. RECOMP_LOCAL_REGS also
keeps integer registers in C locals, and lifted functions keep flag operands,
carry, direction and comparison state in locals. The existing arm64_guest.h
RECOMP_ENTER override only intercepts function entry; adding a dump there would
not make those native continuations persistable. No generated game C was edited
or committed for this experiment.

The next implementation milestone is an optional pcrecomp continuation backend
on synthetic, non-game fixtures: spill each function's live locals/flags into a
frame, replace native nested calls/returns with dispatcher transitions, record
resume labels at call/import/checkpoint boundaries, and preserve existing x86
stack/calling-convention behavior. Test direct/indirect/tail calls, recursion,
branches, flags, x87/SSE and imports against uninterrupted execution before lifting
the game with that backend. Benchmark dispatch overhead before using it by default.
The current handwritten fixture does not prove that generator transformation.

## Host state that must be reconstructed

| Domain | Current source ownership | Required save/restore work |
| --- | --- | --- |
| Guest memory and allocation | guest_memory.h,guest_heap.h | Committed ranges, page protections, heap/free-list metadata; bounded validated mapping restore |
| CPU and continuations | guest_runtime.cpp register state,guest_threads.inc,pcrecomp generated calls | Full integer/flags/x87/MMX/SSE state per thread, explicit call frames, safe import continuations |
| Scheduling and synchronization | guest_threads.inc maps of threads,timers,events,mutexes and critical sections | Stop all workers at safe points, serialize owners/waits/timer deltas, recreate logical handles without pthread pointers |
| Graphics | d3d9_bridge.cpp objects/addresses maps, DXVK/Vulkan native COM objects | Stable guest IDs, object descriptors/content/references/device state; reconstruct GPU resources, rebind a new surface; no native pointer reuse |
| Audio | guest_audio.inc SoundObject/PcmVoice maps and AAudioStream | Voice sample buffers/play positions/loop/volume/pitch; recreate stream and reset output queues coherently |
| Files/search handles | guest_files.inc fileHandles and fileSearches | Paths, access modes, offsets, enumeration cursor; validate underlying files, avoid replaying writes |
| Network | guest_network.inc socket map | Explicit offline-only first milestone; reject live sockets, no claim of resuming server connections |
| Input/time | guest_input.inc, frame pacing, runtime clocks | Release physical input, preserve virtual game time, rebase timers without counting time while the app was dead |

A real format also needs exact executable/generated-code/runtime compatibility,
resource completeness checks, coordinated capture, an atomic published file and
failure recovery that never mutates the live game until validation is complete.
The prototype rejects nonzero host resources rather than pretending to cover them.

## User-visible status

Installed0.14.13 still supports live-memory Resume only. Disk restoration of a
race after Android kills the process or the phone restarts remains unfinished.
Career-save ZIP backups are separate and remain useful for permanent progress.
No snapshot button or misleading snapshot-success message has been added.

## Generated integer backend milestone

The next isolated step is implemented in
experimental/save_states/generate_continuations.py. It reuses pcrecomp's patched
integer instruction lifter and emits explicit PC/frame transitions for control
flow. Seven synthetic x86 programs match Unicorn, including all registers, defined
comparison flags and full guest-memory CRC;191 fresh-process checkpoints match at
every executed instruction. See experimental/save_states/GENERATED_CONTINUATIONS.md
for dependency pins, reproduction and explicit coverage limits. It remains an
experimental integer-only backend, not a lift/restore of NFSU2.

## Selected FP/SIMD and pending-host milestone

The generated proof now serializes selected x87 runtime fields/comparison locals,
MMX/XMM values and control registers. Schema2 rejects old/invalid states. Fourteen
synthetic programs/245 dispatch checkpoints match the x86 reference and fresh
restores. The exact-int64 shadow test preserves9007199254740993. A synthetic host
service can be captured requested or completed-awaiting-return, with no duplicate
snapshot-contained logical effect. This is not an exactly-once external-I/O
protocol and does not restore a real NFSU2 host import. See the generated proof
README for precision/alias/exception and host-side-effect limitations.
