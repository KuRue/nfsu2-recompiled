# Read-only file resource reconstruction (experimental)

`file_resources.h` records logical file handles as `{id, relative path, position, full content}`;
host descriptors are never saved. Restore reopens each path beneath a root descriptor component by
component with `O_NOFOLLOW`, requires a regular file whose bytes equal the captured content, seeks to
the saved position, and publishes handles only after the whole set validates.

Run: `python3 scripts/check_file_resources.py [--sanitize]`

The test captures three handles on one file (mid-file, later, EOF), restores them in a fresh process
with a different descriptor layout, and checks independent offsets and seeking. It rejects every
truncation, trailing bytes, CRC damage, absolute/`..`/`.`/NUL paths, duplicate IDs, out-of-range
positions, changed/shrunk/grown/missing files, and symlinked files or directories.

Limits: read-only files up to 64 KiB, 32 handles. No writable files, write replay/journaling,
memory-mapped files, directory enumeration handles, archive (`.BUN`/`.BIN`) streaming state, or
integration with the game's real file layer.
