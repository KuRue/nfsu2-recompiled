#!/usr/bin/env python3
"""Remove the game's Display category from both Options carousels.

The Android Evolved Graphics panel owns display settings in game, so the
native Display page is not parented. The menu item object is still built above
the patch point, keeping the SEH cleanup state valid.

Run after every lift (scripts/rebuild-run.sh does it automatically).
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TARGET = ROOT / "src/recomp/gen/recomp_0013.c"

FRONTEND = """L_004B62CC:
    edx = MEM32(esi); /* 0x004B62CC: mov edx, dword ptr [esi] */
    PUSH32(esp, edi); /* 0x004B62CE: push edi */
    ecx = esi; /* 0x004B62CF: mov ecx, esi */
    MEM32(esp + 0x1C) = ebx; /* 0x004B62D1: mov dword ptr [esp + 0x1c], ebx */
    RECOMP_ICALL(MEM32(edx + 0x18)); /* 0x004B62D5: call dword ptr [edx + 0x18] */
    RECOMP_FLAGS_IN();"""

FRONTEND_PATCHED = """L_004B62CC:
    /* The in-game Graphics panel owns display settings: the Display category
       is not added to the main Options carousel. The item object is still built
       above so the SEH cleanup state stays valid; it is simply never parented. */"""

PAUSE = """L_004C3108:
    edx = MEM32(esi); /* 0x004C3108: mov edx, dword ptr [esi] */
    PUSH32(esp, edi); /* 0x004C310A: push edi */
    ecx = esi; /* 0x004C310B: mov ecx, esi */
    MEM32(esp + 0x1C) = ebx; /* 0x004C310D: mov dword ptr [esp + 0x1c], ebx */
    RECOMP_ICALL(MEM32(edx + 0x18)); /* 0x004C3111: call dword ptr [edx + 0x18] */
    RECOMP_FLAGS_IN();"""

PAUSE_PATCHED = """L_004C3108:
    /* Same as the frontend Options carousel: no Display entry in the pause
       menu now that the in-game Graphics panel owns every display setting. */"""


def patch(text):
    for original, replacement in ((FRONTEND, FRONTEND_PATCHED), (PAUSE, PAUSE_PATCHED)):
        if replacement in text:
            continue
        if original not in text:
            raise SystemExit("patch point not found; the lift output changed")
        text = text.replace(original, replacement, 1)
    return text


def main():
    if not TARGET.is_file():
        raise SystemExit(f"{TARGET} does not exist; run the lift first")
    text = TARGET.read_text()
    patched = patch(text)
    if patched != text:
        TARGET.write_text(patched)
        print(f"removed Display category from {TARGET}")
    else:
        print("Display category already removed")


if __name__ == "__main__":
    main()
