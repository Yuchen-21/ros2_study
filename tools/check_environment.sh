#!/usr/bin/env bash
set -euo pipefail

command_version()
{
  local command_name=$1
  if command -v "$command_name" >/dev/null 2>&1; then
    local path
    path=$(command -v "$command_name")
    local first_line
    first_line=$("$command_name" --version 2>&1 | head -n 1 || true)
    printf '%-12s %-7s %-28s %s\\n' "$command_name" "FOUND" "$path" "$first_line"
  else
    printf '%-12s %-7s %s\\n' "$command_name" "MISSING" "-"
  fi
}

echo "== Platform =="
uname -a
if [[ -r /etc/os-release ]]; then
  grep -E '^(NAME|VERSION|VERSION_ID)=' /etc/os-release
fi

echo
echo "== Build and observation tools =="
printf '%-12s %-7s %-28s %s\\n' "COMMAND" "STATE" "PATH" "VERSION"
for command_name in \
  g++ cmake colcon ros2 strace ss lsof tcpdump tshark perf gdb; do
  command_version "$command_name"
done

echo
echo "== ROS environment =="
printf 'ROS_DISTRO=%s\\n' "${ROS_DISTRO:-<unset>}"
printf 'RMW_IMPLEMENTATION=%s\\n' "${RMW_IMPLEMENTATION:-<unset: use distro default>}"
printf 'ROS_DOMAIN_ID=%s\\n' "${ROS_DOMAIN_ID:-<unset: default domain>}"

echo
echo "Missing optional observation tools do not block the C++ build."
echo "Stage-specific docs explain which tool applies to each IPC path."
