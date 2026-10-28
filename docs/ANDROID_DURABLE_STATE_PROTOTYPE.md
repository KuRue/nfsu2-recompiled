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

### Stack probe on the phone (2026-10-27)

A build made with `-Pnfs.stackProbe=ON` recorded every lifted call chain that was live at Present and at every
wait (WaitForSingleObject, Sleep and EnterCriticalSection). It ran on the phone through menus and a race:
about 893,000 probe hits and 510 unique chains. `scripts/analyze_stack_probe.py LOG --live-out FILE` turns the log into
the live-function list. That list is in `docs/data/stackprobe-live-functions-2026-10-27.txt`.

- Present has just 4 distinct chains, depth 7 to 9 and 11 functions, so the per-frame capture point is narrow.
- Mutex waits account for 474 chains and 466 functions, mostly CRT and file locks. Captures should be deferred
  while one of those is held, not converted.
- Every chain starts at the thread-entry host callback. No chain contains a second host callback, so no guest
  code is live under a host callback at these points.
- All 521 live functions are catalog functions. Running `continuation_coverage.py --only` on them shows 316
  fully supported (61%) and 97.6% of their instructions supported.
- The main blockers are `movzx` (65), x87 (`fmul` 52, `fcomp` 45, `fadd` 29), `shl` (48), `movsx` (38), `sar`,
  `leave`, `rep stos`/`movs`, `cdq`/`idiv` and `setcc`.

Next step: add these mnemonics to the emitter, starting with integer ones and then x87, and retest on the live set.

### Emitter widened to the live set (2026-10-27)

The emitter now accepts:
- `movzx`, `movsx`, shifts and rotates, `leave`, `cdq`, `mul`, `div`, `idiv`, `xchg` and `setcc`;
- 8-bit and 16-bit registers and memory operands for integer instructions;
- the atomic `rep stos`/`movs` forms;
- the common x87 arithmetic and compare instructions.

Five new fixtures (`root_narrow`, `root_shift`, `root_divide`, `root_string`, `root_x87`) match Unicorn and restore at every
checkpoint, both normally and under ASan/UBSan: 19 programs and 330 checkpoints in total.

The pinned lifter builds the `idiv` dividend with a signed left shift, which UBSan flags. The emitter rewrites that expression
into the unsigned equivalent. The production Android build still contains the original form, which should be fixed upstream in
pcrecomp.

Mnemonic coverage:

| Scope | Functions fully supported | Instructions supported |
|---|---|---|
| Probe live set | 511 of 521 (was 316) | — |
| Whole executable | 25,389 of 27,742 (was 46%) | 99.4% |

A follow-up added `fisub`, `fisubr`, `fidiv`, `fidivr`, `fucom`, `fucomp`, `fucompp` and `stosw` (fixture `root_x87_integer`), for 20 programs and 345 checkpoints. That brings the live set to 517 of 521 and the whole executable to 26,287 of 27,742 (95%). The only remaining live blockers are `int3` padding (3) and `rdtsc` (1, which needs a host-provided value).
This is a mnemonic-level estimate; operand checks and call-target rules can still reject individual functions.

Policy (user-confirmed 2026-10-27): career and profile save files are never rolled back by a save-state restore.
The game state returns to the snapshot, but files written by the game's own career save keep their newest content.

### Real per-function generation (2026-10-27)

`scripts/check_live_generation.py ADDRESS_FILE` runs the emitter itself on each live function rather than counting mnemonics. Calls
to other functions are declared as host services. Tail-jump targets are converted alongside, because a tail jump replaces the frame.
That adds 83 functions reached only by tail jumps.

Progression on the 521 live functions:

| Stage | Functions that generate |
|---|---|
| Mnemonic-only estimate | 517 |
| First real run | 237 |
| Memory-indirect calls and jumps (vtables, switch tables), with run-time target checks | 455 |
| `fs:[0]` accesses for the compiler's exception chain | 455 |
| With the tail-jump closure | 503 |

New fixtures `root_virtual`, `root_switch` and `root_seh` bring the proof to 23 programs and 396 checkpoints, all matching Unicorn,
also under ASan/UBSan. The reference emulator now installs a flat descriptor table plus an `fs` descriptor. Far calls, which
Capstone reports as plain `call`, are rejected by their encoding.

Remaining 18:
- 8 share code with another catalog entry (overlapping ranges);
- 6 fall through their catalog end;
- 3 contain `int3`;
- 1 uses `rdtsc`.

The first 14 need the catalog's chunk boundaries merged. `int3` padding and `rdtsc` need small dedicated handling.

### Catalog chunks and compile check (2026-10-27)

The catalog's `end` field misses cold blocks that the compiler placed after other functions. The emitter now accepts a function as a
list of `(address, bytes)` chunks, and the live checker builds these from the catalog's merged blocks. The entry only has to be an
instruction boundary. `int3` padding is emitted as a fail-closed trap.

Results:
- 520 of 521 live functions generate. Only the 5 genuine tail-jump targets are converted alongside them; before, 83 were, because
  most of the "targets" were cold blocks.
- `check_live_generation.py --compile` builds all 520 units (241k generated lines) with `-Wall -Wextra -Werror`.
- The fixture proof adds positive and negative checks for chunked functions and `int3`.

The only live function left is `0x531df0` (`rdtsc`). It needs a host-provided time value, which is also what step 2's runtime
integration needs.

## Step 2: resumable call sites in the real build (2026-10-27)

The production lifted C can already be resumed at every call site:
- With `RECOMP_LOCAL_REGS`, all guest registers are written to the globals before a call and read back after it.
- The lazy flags pass through `g_flag_*`.
- Only two C locals live in the native frame: `_fpu_cmp` and `_df`.

So the capture-point functions do not need the per-instruction dispatcher. They keep native speed, and a build step
(`scripts/make_resumable.py`) rewrites their call sites into `NFS_RCALL`/`NFS_RICALL`/`NFS_RITAIL`. Each call then records
`{function, site, target, _fpu_cmp, _df}` on a per-thread frame stack. Switch-table fallbacks use the table `jmp` as their site.

The per-instruction emitter stays as the proof harness. Its whitelist no longer limits the game, so `rdtsc` in `0x531df0` is no
longer a blocker.

| Option | Effect |
|---|---|
| `-Pnfs.resumable=ON` (CMake `NFS_RESUMABLE`, default OFF) | Rewrites all 521 probe-live functions (4,719 call sites, about 3.5 s at configure time) |

At every Present and every wait, the runtime checks whether the thread's guest stack consists entirely of recorded frames:
- the bottom frame is the `callGuest` entry;
- each frame's target equals the next frame's function;
- the top frame called the host import directly;
- there is no nested host callback.

It logs `NFSResumable` capturable counts and the top blocking functions every 10 s. Nothing is captured or restored yet; this
measures how often a capture would be possible.

### Phone measurement (2026-10-27)

The `-Pnfs.resumable=ON` build ran on the phone through menus, the garage, a race and free roam for about 8 minutes. Over the
session there were no crashes and no NFSU2 errors.

| Capture point | Capturable | Checks |
|---|---|---|
| Present | 99.996% | 51,068 |
| Sleep | 100% | 130,054 |
| Event wait | 100% | 50,225 |
| Mutex wait | 99.94% | 1,017,022 |
| EnterCriticalSection | 127 of 130 | 130 |

A capture that is not possible can wait for the next frame.

The 14 blocking executable functions were added. `docs/data/resumable-functions.txt` (535 functions, 5,154 call sites) is now the
build's list. One blocker, `0x1102f024`, is in a game DLL module and occurred twice; it is left for later.

### In-process stack rebuild (2026-10-27)

The same resumable build, with `debug.nfs.rebuild=3`, tore down the main thread's native stack at Present and rebuilt it from the
recorded frames. The log prints rebuilds 1–3 and every 64th. Rebuild `#64` was 189.3 s after `#1`, which is a 3.005 s average, so
none of those 64 were skipped. Each one rebuilt 4 frames. The process (`com.nfsu2.androidevolved.dev:game`, pid 17575) then kept
presenting for another 78 s. There was no "Resume failed", no tombstone, and no process restart. The controller layout was not
part of this session.

Final 10-second sample, about 4.5 minutes after the first Present:

| Capture point | Capturable | Checks | Blockers |
|---|---|---|---|
| Present | 100% | 26,869 | none |
| Sleep | 100% | 51,733 | none |
| Event wait | 100% | 28,519 | none |
| Mutex wait | 99.959% | 610,426 | `0x481fd0` 213, `0x46ae00` 27, `0x463c80` 6, DLL `0x1102f024` 5 |
| EnterCriticalSection | 95 of 98 | 98 | `0x57d8c0` 3 |

The four executable addresses are catalog entries (51, 283, 385 and 54 bytes). During that session they were not in the resumable
list yet. Their counts stopped rising, so each was a short burst rather than a stuck thread. Present stayed fully capturable,
which is why the rebuild could fire on time. Evidence: `build/verification/rebuild-2026-10-27.log`. The property was cleared and
the dev process stopped after the session, so a later launch does not keep rebuilding.

### Capture file (2026-10-27)

Those four addresses are now in `docs/data/resumable-functions.txt` (539 addresses). `make_resumable.py` accepts their call forms:
4 functions, 20 call sites. DLL `0x1102f024` is not an executable function and stays off the list. A capture waits for the next
clean frame instead of recording that call.

At Present the presenting thread holds the machine lock, so every other live thread is already blocked in a yield or in
`released()`. The capture still requires an explicit park, published before the lock is dropped:

- a thread inside guest code must be at a resumable Sleep, event, mutex, thread, multiple-object, critical-section, message, or socket wait;
- the timer idle loop and the pre-start suspend loop are outside guest code, so they park as `TimerIdle` or `Suspended` with no frames;
- a time-slice yield inside a guest function is not a capture point.

`SleepEx` and `GetMessage` now use the same wait label as the other waits. The network path publishes its park inside `released()`
before unlocking. `debug.nfs.rebuild` is unchanged. If both properties are set, the rebuild test throws first and the capture runs
only when that test returns.

`adb shell setprop debug.nfs.capture 1` writes one file, then stops until the property is set back to 0. The file is
`<game root>/captures/capture-<unix seconds>.nfs` (on the phone, `/sdcard/nfsu2/captures/`). Magic `NFSCAP1`, schema 1. It stores
the build id, the SPEED2.EXE SHA-256, each thread's registers and resumable frames, logical events, mutexes and timers, committed
guest memory, and protection exceptions. `careerRollback` is 0 and a decoder rejects anything else. The omitted mask must keep
graphics, audio, heap metadata, and file contents. Open-file, sound, and D3D counts are inventory only; they do not make those
objects restorable. Career and profile files are not opened. This file is not a restore.

Host check: `tests/capture_quiesce.cpp`, run by `scripts/check_phone_source.py`. The runtime translation unit compiles with the
NDK both with and without `NFS_RESUMABLE`. A phone file does not exist until a resumable build is installed and the property is
set during play.

### Restore inventory: host state behind guest handles (2026-10-28)

A capture already holds guest memory, thread registers and frames, and the logical events, mutexes and timers. Everything else the guest refers to is a handle whose meaning lives in a host table, and a fresh process must rebuild each table:

| Host table | Guest sees | Data lives | Restore |
|---|---|---|---|
| `GuestHeaps` (`guest_heap.h`) | heap pointers | host block maps | serialize the maps |
| `fileHandles` (`guest_files.inc`) | `0xF006xxxx` handles | host fds; the path is not kept | keep path, access and offset at open; reopen (proved in `check_file_resources.py`); career files are never rolled back |
| `soundObjects` (`guest_audio.inc`) | proxy objects with a 24-entry method table in guest memory | voice samples, position, volume, pan, frequency on the host | save voices (proved in `check_audio_state.py`) |
| `objects` (`d3d9_bridge.cpp`) | proxy objects with method tables of `0x7E00xxxx` tokens in guest memory | native DXVK objects; contents mostly on the GPU | recreate each object with its creation parameters, then its contents |

Direct3D is the open design question. Guest memory restores the proxies unchanged, so only the address-to-native map and the contents need rebuilding. Proposed split, to be checked against a race-time inventory:
- managed and system-memory pool resources: read back through DXVK at capture time and store;
- default-pool render targets, depth buffers and dynamic buffers: determine which contents survive a frame boundary; recreating these empty is not proven safe;
- default-pool static buffers, if any: shadow the bytes on Unlock.

The device itself (render states, transforms, bound streams and textures, shaders) is replayed from a recorded state block (proved in `check_graphics_state.py`). Next measurement: object counts and bytes per kind and pool during a race.

### Load within a session (2026-10-28, not yet run on the phone)

`debug.nfs.load <path>` (resumable build) loads a capture into the session that wrote it. Graphics contents are not stored, so the load first requires the live D3D9 objects to equal the capture's list, the build and executable digest to match, and every live thread to be in the capture with the same role and entry function. Any mismatch logs `load refused, nothing changed` under `NFSCapture`.

It then restores, in order: page protections opened, guest memory regions, captured protections, heap bookkeeping, open files (reopened by path at their position; never created or truncated, so career files keep their newest contents), DirectSound objects (voices rebuilt from the guest sample copies), events, mutexes, timers (due times relative to now) and the set of initialized critical sections. Each parked worker gets a pending rewind and takes it in `yieldGuestMachine` when it next holds the machine: it throws the same `RebuildRequest` as the stack-rebuild test and resumes from its captured frames, re-issuing its wait. The presenting thread rebuilds at once.

Critical sections now lock on their guest fields (recursion at +8, owner at +12) instead of host recursive mutexes, so rewinding guest memory rewinds lock ownership.

Schema 3 also stores all 1,088 TLS allocation flags, each thread's 1,088 slot values and its
`lastError`. The presenting thread applies its local state before rebuilding; parked workers
apply theirs under the machine lock when their wait returns, including workers idle outside
guest code. Socket waits take pending loads after reacquiring the machine too. A second
capture/load waits until all pending worker state has been consumed. Schemas 1/2 remain
readable for inspection, but the loader rejects them before changing anything because their
thread-local state is missing. Slot values are kept even for unallocated slots, matching the
runtime's existing TLS behavior.

Graphics contents remain unrestored (hence same-session only). The TLS changes passed format
round trips, malformed-input checks and host thread isolation/restore checks; they have not
been exercised by loading a capture on the phone.

Phone test:
1. Install `assembleDebug -Pnfs.resumable=ON`, start a race.
2. `adb shell setprop debug.nfs.capture 1`; wait for `NFSCapture: wrote .../captures/capture-N.nfs`; `setprop debug.nfs.capture 0`.
3. Drive on for a few seconds, then `adb shell setprop debug.nfs.load /sdcard/nfsu2/captures/capture-N.nfs`.
4. Expect `loaded ...; rewinding` and the race back at the capture moment, or a refusal naming the reason.


### Immutable graphics programs (offline, 2026-10-28)

`d3d9_program_state.h` captures shader bytes through native `GetFunction` and declaration
entries through `GetDeclaration`, with bounded sizes and token/declaration checks. This
replaces the creation recorder's guest-pointer scan: an END value inside a shader comment
or constant must not truncate the definition. Program creation records clear their input
and output pointer arguments and keep an independent byte copy. A recording failure logs
and leaves the definition unavailable; the successful game Create call still succeeds.

The recreation helper validates a definition, calls the device's Create method with aligned
storage, and returns an owned staged native object. Failure or abandonment releases it;
a later complete graphics restore can transfer ownership only after all resources succeed.
The helper has not been wired to the snapshot loader, and program definitions are not yet
serialized in NFSCAP1. Guest proxy addresses/reference counts, device bindings/state blocks,
textures, buffers and derived surfaces still need capture/reconstruction and a coordinated
commit. No claim of cross-session graphics restore is made by this step.

`tests/android_graphics/run.sh` now includes program capture/recreation and bridge record
checks, using mock COM objects/device. Cases cover shader models 1/2/3, comment and constant
END values, declaration sentinels, truncated/oversized definitions, driver failure/null output,
input pointer removal and staged object lifetime. This verifies the shared helper and recorder,
not DXVK rendering or a race. Run against a real device when available.
