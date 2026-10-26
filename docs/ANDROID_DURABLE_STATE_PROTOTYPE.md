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

## Logical scheduler and event/timer milestone

experimental/save_states/LOGICAL_SCHEDULER.md documents the next proof: complete
logical process state with per-thread CPU images, event/timer handles, virtual time,
wait deadlines and deterministic round-robin position. Six scenarios/96 fresh
restores match uninterrupted process state; a blocked deadlock reports cleanly
before/after restore. This is not integration with guest_threads.inc or capture of
real native workers. File, audio and graphics resource reconstruction remains open.

## Real-game coverage and the hybrid plan (2026-10-26)

All standalone proofs now exist: CPU continuations, scheduler with events/timers/mutexes,
read-only and writable files (FILE_RESOURCES.md), audio mixer voices (AUDIO_STATE.md) and a
logical graphics device (GRAPHICS_STATE.md). None is integrated into the game.

`scripts/continuation_coverage.py` measures the experimental emitter against the real
executable: 12,853 of 27,742 functions (46%) use only proven instructions; 88% of decoded
instructions are covered. There are 245 unsupported mnemonics; the most common are `leave`, shifts,
`movzx/movsx`, x87 arithmetic/compares, `rep movs/stos`, `setcc`, `div`, `cdq`. The production lifter
already handles all of them (zero lift failures), so closing this is mostly whitelisting each
instruction group with x86/Unicorn differential tests, not new semantics.

Converting all 27,742 functions to dispatcher form is unnecessary and would cost speed. Only the
functions that can be *on a thread's stack at the capture point* need explicit continuations;
everything they call that returns before capture can stay ordinary native C. Plan:

1. Choose capture points: the main thread at its per-frame Present call, every worker blocked
   in a wait/sleep import. Capture only when every guest thread is at one of them.
2. Record, on the phone, the guest call stacks at those points (RECOMP_ENTER hook / the
   runtime's shadow return addresses) across menus, races and loading. That gives the small
   function set needing continuation form.
3. Lift only that set with the continuation backend; keep a runtime guard that
   defers capture to the next frame if any native-C frame is still live.
4. Wire guest_threads.inc, guest_files.inc, guest_audio.inc and d3d9_bridge.cpp to the logical
   models; reject capture while sockets are open (offline only).
5. Restore into a fresh process; career/profile save files are never rolled back.

Step 2 needs the phone connected; steps 1-3 decide whether the approach is affordable.
