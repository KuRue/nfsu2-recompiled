# Durable continuation prototype (not an NFSU2 save-state feature)

Run from the repository root:

```sh
python3 scripts/check_snapshot_prototype.py
python3 scripts/check_snapshot_prototype.py --sanitize
```

This is an isolated, handwritten integer program using the production GuestMemory
address-space class. It reads no game files or career saves, runs no Android app,
and changes no production runtime behavior. The runner builds in a temporary
folder, launches separate baseline/capture/restore processes, and cleans up the
binary/state files. The capture process exits before restoration starts. No host
stack or heap pointer is serialized. It requires Linux, Python3 and C++17.

## Verified result

The root calls a loop that calls a leaf. Capture happens in the middle of the
50th leaf call, with three explicit frames,49 completed ticks and register sum3725.
Restoring into a fresh GuestMemory mapping completes the same computation as the
uninterrupted run: result71787,ticks100,flags0. The expected result is also checked
against an independent sum formula. Frame function IDs, continuation PCs, local
loop state, eight integer register slots, a flag word and4096 bytes of guest
memory survive. This demonstrates how explicit continuations solve the native
call-stack problem; it does not transform actual lifted game functions.

The codec checks every truncated prefix, corrupted payload, unknown format,
wrong build/game IDs, unexpected trailing data, overlapping/wrapping ranges,
empty continuations and unsupported host resources. A rejected capture leaves
an existing checkpoint unchanged and does not leave a staging file. The script
also tests truncated-file rejection across a fresh process. Tests do not simulate
power failure, concurrent capture, graphics, audio, networking or real game threads.

## Experimental format boundary

N F S P R O T O magic, little-endian schema2, payload length and CRC32 precede a
bounded payload. Exact64-character build/game identifiers are required. The test
build ID is the executable SHA256; its game ID is a synthetic constant, not the
SPEED2.EXE hash. Maximum payload memory is16MiB, with64 continuation frames and128
nonoverlapping regions. Those limits are for this proof, not sufficient for a
full game. CRC detects accidental corruption; it is not authentication.

Publication writes a unique0600 staging file, syncs it, renames it over the target,
and syncs the parent directory. Invalid state is rejected before staging begins.
Only validated decoded state is applied to the fixture's new memory mapping.
The host-resource mask must be zero: graphics/audio/file/thread state must never
be silently omitted from a real capture. Schema2 also stores selected x87/MMX/SSE runtime fields and a pending synthetic
host-call descriptor. It still has no allocator metadata, protected-page metadata
or guest scheduler. Schema1 files are rejected rather than guessed/migrated.

State files are ignored by Git. They must never be attached publicly if they
contain proprietary guest memory in a future implementation. This format is
experimental and has no compatibility promise or launcher button.

## Generated continuation backend proof

The next proof now consumes synthetic x86 machine code and reuses the existing
patched pcrecomp integer instruction lifter, instead of handwritten dispatch.
See [generated proof](GENERATED_CONTINUATIONS.md) for setup and coverage. It has a
separate pinned-dependency/reference-test job in CI. Neither experiment is wired
into Android's game generation or save UI.
