#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd "$script_dir/.." && pwd)
source_dir="$repo_root/labs/01_linux_ipc/linux_ipc_lab"
build_dir="${STAGE1_BUILD_DIR:-$repo_root/build/stage1}"

if [[ -n "${BUILD_JOBS:-}" ]]; then
  build_jobs=$BUILD_JOBS
else
  build_jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)
fi

cmake \
  -S "$source_dir" \
  -B "$build_dir" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build "$build_dir" --parallel "$build_jobs"
# cmake -E chdir also works with the older CTest shipped by Ubuntu 20.04.
cmake -E chdir "$build_dir" ctest --output-on-failure

echo "Stage 01 build and tests passed: $build_dir"
