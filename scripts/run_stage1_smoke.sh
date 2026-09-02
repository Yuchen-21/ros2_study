#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd "$script_dir/.." && pwd)
build_dir="${STAGE1_BUILD_DIR:-$repo_root/build/stage1}"

if [[ ! -x "$build_dir/thread_shared_state" ]]; then
  echo "Stage 01 is not built; run ./scripts/build_stage1.sh first." >&2
  exit 2
fi

echo "[1/4] Thread shared state"
"$build_dir/thread_shared_state" --iterations 5000

echo "[2/4] Process virtual-memory isolation"
"$build_dir/process_memory_isolation"

echo "[3/4] Anonymous pipe across fork+exec"
"$build_dir/pipe_process_a" --count 5 --interval-ms 1

echo "[4/4] UDS, TCP, UDP and POSIX shared-memory pairs"
bash \
  "$repo_root/labs/01_linux_ipc/linux_ipc_lab/test/run_ipc_pairs.sh" \
  "$build_dir"

echo "Stage 01 smoke test: PASS"
