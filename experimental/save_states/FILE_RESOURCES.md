# File resource reconstruction (experimental, schema 2)

`file_resources.h` records logical file handles as `{id, relative path, position, full content}`;
host descriptors are never saved. Restore reopens each path beneath a root descriptor component by
component with `O_NOFOLLOW`, requires a regular file whose bytes equal the captured content, seeks to
the saved position, and publishes handles only after the whole set validates.

Run: `python3 scripts/check_file_resources.py [--sanitize]`

The test captures three handles on one file (mid-file, later, EOF), restores them in a fresh process
with a different descriptor layout, and checks independent offsets and seeking. It rejects every
truncation, trailing bytes, CRC damage, absolute/`..`/`.`/NUL paths, duplicate IDs, out-of-range
positions, changed/shrunk/grown/missing files, and symlinked files or directories.

## Writable handles

A writable handle's saved bytes are authoritative: the snapshot rolls the file back to its
captured contents, discarding anything written after capture. Restore stages each replacement
as a temporary file next to the target, validates every read-only file, then renames the
replacements into place (`renameat`, which replaces a symlink rather than following it) and
reopens all handles. Handles on one path must carry identical captured bytes and end up on the
same inode. The target must already exist as a regular file. A failed read-only check publishes
nothing. The test confirms a write through the restored handle lands at the saved offset and is
visible through a second read-only handle. It also rejects symlinked, directory and missing
writable targets without touching anything outside the root.

Limits: files up to 64 KiB, 32 handles. Publishing several writable files is not atomic as a
group (a crash between renames can leave a mix). No write replay/journaling, memory-mapped files, directory enumeration handles, archive (`.BUN`/`.BIN`) streaming state, or
integration with the game's real file layer.
