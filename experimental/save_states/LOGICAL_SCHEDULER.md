# Checkpointable logical thread scheduler proof

Run with the same pinned pcrecomp checkout and Python environment as the generated
continuation proof:

```
build/continuation-venv/bin/python scripts/check_logical_scheduler.py
build/continuation-venv/bin/python scripts/check_logical_scheduler.py --sanitize
```

This is an opt-in synthetic process model, not the game's pthread scheduler. It
runs generated guest instructions cooperatively and stores a complete CPU image
for each logical thread, including FP/SIMD state. It never serializes a pthread,
condition variable, native stack, host pointer or operating-system handle.

## Scheduler and handle model

Process state includes virtual time, round-robin cursor, monotonic handle allocator,
logical thread IDs/status/waits/deadlines, events and timers. Event IDs are stable
across a fresh process. Timers refer to those IDs, not pointers. Manual-reset events
release all waiting threads and remain set; auto-reset events release one waiter
in serialized creation order and consume their signal. Timer events are processed
before timeouts at the same virtual tick. Timers may be one-shot or periodic.

Synthetic service IDs2/3/4 represent wait-event, set-event and sleep. Their calls
use the previously implemented requested/completed-return protocol. A blocked
thread retains its requested import and absolute virtual deadline; restoring it
does not restart the timeout. Wakeup changes it to runnable with a staged return
value (0 or timeout0x102). Finite zero-time waits poll without blocking; infinite
waits use0xffffffff. A finished thread has a canonical non-executable terminal
frame, never dispatched as guest code.

Each successful instruction/service turn advances virtual time by one tick. If
all threads are blocked, time jumps to the next logical timer/deadline. No wall
clock is consulted, so time while the capturing process is dead is not added to
game time. A graph with no runnable thread or future wakeup reports deadlock rather
than hanging. This is a deterministic test policy, not a Windows scheduling or
priority guarantee.

## Process snapshot and validation

A separate NFSSCHED/schema1 envelope embeds the existing schema2 CPU capsules.
It records events, timers, thread state, virtual time, cursor and handle allocator,
with bounded length and CRC32. Each capsule includes the same shared guest-memory
image; images must agree before publication and after decoding. This intentionally
duplicates memory to keep the proof small. It is not a production storage layout.
Maximums are8 threads,32 events,32 timers,16MiB envelope and1,000,000 virtual ticks.

Handle IDs must be unique and correctly typed. Timers must reference existing
events; blocked waits must match their pending import, event and deadline. IDs
cannot be reused by a restored allocator. Finished/runnable threads cannot retain
wait records. A snapshot is completely decoded and its graph validated before
its memory is installed. Atomic file publication uses the common staging/fsync/
rename helper. No user game file is read, restored or overwritten.

## Verified scenarios

| Scenario | Fresh-process checkpoints | Final virtual time |
| --- | ---: | ---: |
| Auto-reset event, waiter plus delayed signaler |22 |22 |
| Manual-reset event, two waiters plus delayed signaler |31 |31 |
| One-shot timer wakes an event waiter |11 |25 |
| Periodic timer, two successive event waits |16 |44 |
| Finite event-wait timeout |8 |9 |
| Sleep with no other runnable thread |8 |10 |

Total96 checkpoints cover runnable, blocked, sleeping and finished threads, and
captures where all threads are blocked. The waiter and signaler hold different
live x87 values over context switches and waits; final1.0/2.25 memory outputs are
checked independently. Manual/auto event signal counts and flags, timeout0x102,
sleep result42 and completion counters have independent expected outcomes.
Restored runs must match the entire final process-state CRC and virtual clock
of an uninterrupted run, not only the visible result. These are deterministic
scheduler references, not a comparison with actual concurrent Windows execution.

A separate permanently blocked scenario is captured while waiting forever; both
the uninterrupted and restored process must report deadlock. Negative tests reject
duplicate IDs, invalid allocator/cursor/status, unknown or wrongly typed handles,
expired timers, inconsistent shared memory and invalid flags. Decoder tests include
recomputed checksums on malformed graphs, wrong build identities, truncation and
corruption, so a valid CRC alone is not sufficient.

The extended sanitizer fixture exposed the pinned lifter's direct unaligned
double/float casts (rather than its memory macros). The experimental emitter now
uses byte-aligned alias access types for those emitted casts too. The source data
remains unaligned, exercising that path. Pinned pcrecomp files and production
Android code were not changed. CI runs the scheduler proof with fail-fast ASan/UBSan.

## Remaining boundary

Actual NFSU2 save states are still unimplemented. This proof has no production
thread priorities, critical sections/mutex ownership, callbacks, exception/longjmp
continuations, file descriptors/search cursors, audio voices/streams, D3D/Vulkan
resources, sockets, heap metadata or page protections. It does not capture running
native workers concurrently; captures happen only between logical turns. Real
integration needs a coordinated quiescence contract and reconstruction of each
supported resource class before the game can restore a race. Existing live-memory
Resume and the installed0.14.13 app are unchanged.

## Mutexes (schema 2)

Logical mutexes share the handle namespace and save `{id, owner thread, recursion, abandoned}`.
Synthetic services: 5 = acquire (handle, timeout; 0 polls, `0xffffffff` infinite), 6 = release.
Acquire is recursive for the owner; contended acquire blocks with optional timeout (`0x102`).
Release by a non-owner returns 0. The final release hands ownership to the first waiter in
thread-creation order. A thread that finishes while owning a mutex abandons it: the next acquirer
gets `0x80` (WAIT_ABANDONED) and becomes owner. Validation rejects owners that are missing or finished,
owner/recursion mismatches, abandoned-but-owned mutexes, and waiters on free or self-owned mutexes
(lost wake-ups).

Scenarios: `mutex` (three recursive lockers serialize a read-sleep-write; extra release fails),
`abandoned` and `mutex_timeout` (poll and timed acquire fail, then infinite acquire succeeds after release):
175 extra fresh-process checkpoints. Critical sections are expected to map to the same model
(they also allow recursive entry and have one owner). Spin counts, priority, and real Win32
fairness are not modeled.
