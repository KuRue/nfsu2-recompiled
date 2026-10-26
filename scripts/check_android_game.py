#!/usr/bin/env python3
"""Read-only preflight for the exact PC game build used by this native port."""
import argparse
import hashlib
from pathlib import Path

EXPECTED_SIZE = 4_800_512
EXPECTED_SHA256 = "f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7"
DIRECTORIES = ("CARS", "CREDITS", "FRONTEND", "GLOBAL", "LANGUAGES", "MOVIES",
               "NIS", "SOUND", "SUBTITLES", "TRACKS")


def check(root):
    problems = []
    exe = root / "SPEED2.EXE"
    try:
        if exe.stat().st_size != EXPECTED_SIZE:
            problems.append("SPEED2.EXE has the wrong size; expected v1.2 North America (4,800,512 bytes).")
        else:
            digest = hashlib.sha256()
            with exe.open("rb") as stream:
                for block in iter(lambda: stream.read(1024 * 1024), b""):
                    digest.update(block)
            if digest.hexdigest() != EXPECTED_SHA256:
                problems.append("SPEED2.EXE SHA-256 differs from the supported build; do not generate code from it.")
    except OSError as error:
        problems.append(f"Cannot read {exe}: {error.strerror}")
    for name in DIRECTORIES:
        directory = root / name
        try:
            if not any(entry.name and not entry.name.startswith(".") for entry in directory.iterdir()):
                problems.append(f"Empty game directory: {directory}")
        except OSError as error:
            problems.append(f"Cannot read {directory}: {error.strerror}")
    return problems


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("game_root", type=Path, help="Full installed PC game folder (not just SPEED2.EXE)")
    args = parser.parse_args()
    problems = check(args.game_root.expanduser().resolve())
    for problem in problems:
        print("ERROR:", problem)
    if not problems:
        print("Supported executable and nonempty game directories found. Asset integrity and gameplay still need testing.")
    return 1 if problems else 0


if __name__ == "__main__":
    raise SystemExit(main())
