#!/usr/bin/env python3
"""Remove the in-game Display category from both Options carousels.

The launcher is the single source of truth for display settings. The frontend
Options screen (guest 0x4B6230) and the pause-menu Options screen (guest
0x4C3000) are built in SPEED2.EXE and each append an Audio, Display, Gameplay
item list. The Display screen itself is still in the game data, but with no
carousel entry it is unreachable.

The lifter emits the item construction followed by ``call [edx+0x18]`` (append
item). Keep the construction so the SEH cleanup state stays valid, and drop
only the append call. This is intentionally applied after lifting because
``src/recomp/gen`` is generated and gitignored.

Run after ``scripts/run_lift.py lift ...`` (or use ``RELIFT=1`` with
``scripts/rebuild-run.sh``).
"""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_GEN = ROOT / "src/recomp/gen"

# Guest labels from the lifted code. The comment text is also the idempotency
# marker, so a second run never duplicates the replacement.
LABELS = {
    "L_004B62CC": "main Options carousel",
    "L_004C3108": "pause-menu Options carousel",
}


def patch(text: str) -> tuple[str, int]:
    changed = 0
    for label, where in LABELS.items():
        start = text.find(label + ":\n")
        if start < 0:
            continue
        # The item append call is the last statement in this block; the next
        # item starts with its 0x48-byte allocation.
        end = text.find("PUSH32(esp, 0x48u);", start)
        if end < 0:
            continue
        block = text[start:end]
        if "RECOMP_ICALL(MEM32(edx + 0x18));" not in block:
            continue  # already patched (either comment variant)
        add_end = block.find("RECOMP_FLAGS_IN();")
        if add_end < 0:
            continue
        add_end += len("RECOMP_FLAGS_IN();\n")
        replacement = (
            f"{label}:\n"
            f"    /* {where}: Display category removed. The item is still built\n"
            f"       above so the SEH cleanup state stays valid, but it is never\n"
            f"       parented into the carousel. The launcher owns display settings. */\n"
        )
        text = text[:start] + replacement + text[start + add_end:]
        changed += 1
    return text, changed


def main() -> int:
    gen = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_GEN
    files = sorted(gen.glob("recomp_*.c"))
    if not files:
        print(f"error: no recomp_*.c in {gen}", file=sys.stderr)
        return 1

    total = 0
    found: set[str] = set()
    for path in files:
        text = path.read_text()
        if "0x4204D527u" not in text:
            continue
        new, n = patch(text)
        if n:
            path.write_text(new)
            total += n
            try:
                shown = path.relative_to(ROOT)
            except ValueError:
                shown = path
            print(f"patched {shown} ({n} carousel(s))")
        for label in LABELS:
            if label in new:
                found.add(label)

    missing = set(LABELS) - found
    if missing:
        print(
            "error: carousel labels not found: " + ", ".join(sorted(missing)),
            file=sys.stderr,
        )
        return 1
    if total == 0:
        print("Display menu already removed from generated carousels")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
