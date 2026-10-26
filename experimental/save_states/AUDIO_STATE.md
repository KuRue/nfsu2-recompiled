# Logical audio mixer state (experimental)

`audio_state.h` saves a mixer as block index, voice-ID allocator, and per voice
`{id, buffer, 16.16 position, 16.16 pitch step, volume, loop, playing}`. Sample data is never
saved; each referenced buffer is listed with a CRC and must match the reloaded asset exactly.

Run: `python3 scripts/check_audio_state.py [--sanitize]`

A deterministic synthetic mixer (looping engine voice, pitched one-shot that ends mid-block,
clipping voice, pitch/volume change, stop, late voice) renders 16 blocks of 256 samples.
Captures at every block boundary 0-16 restore in a fresh process and render PCM identical,
block by block, to the uninterrupted run. 157 bad inputs are rejected: every truncation,
trailing bytes, CRC damage, an edited asset, unknown buffers, positions past the end of a playing
voice, invalid pitch/volume/loop, and duplicate or unallocated voice IDs.

Limits: integer mono mixer only; captures happen between blocks. Not covered: the game's real
audio API (DirectSound/EAX), hardware or OpenSL/AAudio buffer latency (queued but unplayed
audio is dropped and regenerated), streamed music decoder state, effects/reverb tails, and
3D positional parameters.
