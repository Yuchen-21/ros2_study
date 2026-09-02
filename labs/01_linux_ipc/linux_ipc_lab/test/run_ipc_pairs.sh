#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 <directory-containing-ipc-executables>" >&2
  exit 2
fi

bin_dir=$1
required=(
  unix_domain_subscriber unix_domain_publisher
  tcp_subscriber tcp_publisher
  udp_subscriber udp_publisher
  shared_memory_writer shared_memory_reader
)
for executable in "${required[@]}"; do
  if [[ ! -x "$bin_dir/$executable" ]]; then
    echo "missing executable: $bin_dir/$executable" >&2
    exit 2
  fi
done

run_dir=$(mktemp -d /tmp/ros2_ipc_lab_test.XXXXXX)
cleanup()
{
  while read -r child_pid; do
    [[ -n "$child_pid" ]] || continue
    kill "$child_pid" 2>/dev/null || true
  done < <(jobs -pr)
  wait 2>/dev/null || true
  rm -rf -- "$run_dir"
}
trap cleanup EXIT INT TERM

wait_for_file()
{
  local path=$1
  for _ in $(seq 1 100); do
    [[ -e "$path" ]] && return 0
    sleep 0.01
  done
  echo "timed out waiting for $path" >&2
  return 1
}

assert_log()
{
  local pattern=$1
  local path=$2
  if ! grep -q -- "$pattern" "$path"; then
    echo "expected pattern '$pattern' in $path" >&2
    sed -n '1,120p' "$path" >&2
    return 1
  fi
}

count=5
uds_path="$run_dir/uds.sock"
"$bin_dir/unix_domain_subscriber" \
  --path "$uds_path" --count "$count" --timeout-ms 3000 \
  >"$run_dir/uds_subscriber.log" 2>&1 &
server_pid=$!
wait_for_file "$uds_path"
"$bin_dir/unix_domain_publisher" \
  --path "$uds_path" --count "$count" --interval-ms 1 \
  >"$run_dir/uds_publisher.log" 2>&1
wait "$server_pid"
assert_log "uds_subscriber complete samples=5 gaps=0" "$run_dir/uds_subscriber.log"

tcp_port=$((41000 + $$ % 1000))
"$bin_dir/tcp_subscriber" \
  --port "$tcp_port" --count "$count" --timeout-ms 3000 \
  >"$run_dir/tcp_subscriber.log" 2>&1 &
server_pid=$!
"$bin_dir/tcp_publisher" \
  --port "$tcp_port" --count "$count" --interval-ms 1 --connect-timeout-ms 3000 \
  >"$run_dir/tcp_publisher.log" 2>&1
wait "$server_pid"
assert_log "tcp_subscriber complete samples=5 gaps=0" "$run_dir/tcp_subscriber.log"

udp_port=$((43000 + $$ % 1000))
"$bin_dir/udp_subscriber" \
  --port "$udp_port" --count "$count" --timeout-ms 3000 \
  >"$run_dir/udp_subscriber.log" 2>&1 &
server_pid=$!
# UDP has no handshake; give the receiver time to bind before the first datagram.
sleep 0.05
"$bin_dir/udp_publisher" \
  --port "$udp_port" --count "$count" --interval-ms 1 \
  >"$run_dir/udp_publisher.log" 2>&1
wait "$server_pid"
assert_log "udp_subscriber complete samples=5 gaps=0" "$run_dir/udp_subscriber.log"

shm_name="/ros2_comm_lab_test_$(id -u)_$$"
"$bin_dir/shared_memory_writer" \
  --name "$shm_name" --count "$count" --interval-ms 1 --timeout-ms 3000 \
  >"$run_dir/shm_writer.log" 2>&1 &
writer_pid=$!
"$bin_dir/shared_memory_reader" \
  --name "$shm_name" --count "$count" --timeout-ms 3000 \
  >"$run_dir/shm_reader.log" 2>&1
wait "$writer_pid"
assert_log "shm_reader complete samples=5 skipped_generations=0" "$run_dir/shm_reader.log"
assert_log "shm_writer complete samples=5 unlinked=true" "$run_dir/shm_writer.log"

echo "ipc_pairs_integration_test: PASS"
