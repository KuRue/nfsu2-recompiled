#!/usr/bin/env bash
set -euo pipefail
test_dir="$(cd "$(dirname "$0")" && pwd)"
task_build="$HOME/.cache/nfsu2-graphics-memory"
cmake -S "$test_dir" -B "$task_build" -G Ninja -DCMAKE_CXX_COMPILER=clang++
cmake --build "$task_build" -j2
"$task_build/graphics_memory_check"
"$task_build/graphics_com_check"
python3 - "$task_build/graphics_program_check" <<'PYTEST'
import subprocess
import sys
import tempfile
with tempfile.TemporaryDirectory(prefix="nfs-program-archive-") as directory:
    subprocess.run([sys.argv[1], "--write", directory], check=True)
    subprocess.run([sys.argv[1], "--read", directory], check=True)
PYTEST
python3 - "$task_build/graphics_buffer_check" <<'PYTEST'
import subprocess
import sys
import tempfile
with tempfile.TemporaryDirectory(prefix="nfs-buffer-archive-") as directory:
    subprocess.run([sys.argv[1], "--write", directory], check=True)
    subprocess.run([sys.argv[1], "--read", directory], check=True)
PYTEST
