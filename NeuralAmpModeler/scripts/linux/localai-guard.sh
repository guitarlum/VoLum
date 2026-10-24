#!/usr/bin/env bash
# Exit 0 when the box is free for a VoLum job.
# Exit 3 and print "BOX_BUSY_LOCALAI <reason>" when localai-platform is busy.
#
# Busy means a process command line matches speed_matrix, bench/run,
# fetch_models, build_llama or pytest, or llama-server used more than 10% of
# one core over 5 seconds (utime+stime from /proc/<pid>/stat). The persistent
# proxy.py process is not busy. This script only reads /proc. It does not
# signal, stop or reconfigure any process.
#
# Invoke as `bash localai-guard.sh` or `bash -s` (script on stdin). Do not pass
# the script body to `bash -c`: that puts these pattern strings on a command
# line, and the scan would treat this script as busy.
set -u

busy_name() {
  case "$1" in
    *speed_matrix*|*bench/run*|*fetch_models*|*build_llama*|*pytest*) return 0 ;;
  esac
  return 1
}

# Bash cannot store NUL, so argv past argv[0] is invisible without tr.
cmdline_of() {
  local pid=$1
  [ -r "/proc/$pid/cmdline" ] || return 1
  tr '\0' ' ' < "/proc/$pid/cmdline" 2>/dev/null || return 1
}

# /proc/<pid>/stat: pid (comm) state ppid ... utime stime
# comm can contain spaces and parentheses, so the fields we want start after
# the last ')'. utime is field 12 and stime field 13 of what remains.
cpu_ticks() {
  local stat rest state ppid pgrp sess tty tpgid flags minflt cminflt majflt cmajflt utime stime
  [ -r "/proc/$1/stat" ] || return 1
  stat=$(<"/proc/$1/stat") || return 1
  rest=${stat##*)}
  read -r state ppid pgrp sess tty tpgid flags minflt cminflt majflt cmajflt utime stime _ <<<"${rest}"
  [ -n "${utime:-}" ] && [ -n "${stime:-}" ] || return 1
  echo $((utime + stime))
}

for dir in /proc/[0-9]*; do
  pid=${dir#/proc/}
  cmd=$(cmdline_of "$pid" || true)
  if [ -r "/proc/$pid/comm" ]; then
    cmd="${cmd} $(<"/proc/$pid/comm")"
  fi
  [ -n "${cmd// /}" ] || continue
  if busy_name "$cmd"; then
    printf 'BOX_BUSY_LOCALAI process matches busy pattern: %s\n' "$cmd"
    exit 3
  fi
done

declare -A start=()
for dir in /proc/[0-9]*; do
  pid=${dir#/proc/}
  cmd=$(cmdline_of "$pid" || true)
  case "$cmd" in
    *llama-server*) ;;
    *) continue ;;
  esac
  ticks=$(cpu_ticks "$pid" || true)
  [ -n "${ticks:-}" ] || continue
  start[$pid]=$ticks
done

if [ "${#start[@]}" -eq 0 ]; then
  exit 0
fi

sleep 5
clk=$(getconf CLK_TCK)
limit=$((5 * clk))
for pid in "${!start[@]}"; do
  ticks=$(cpu_ticks "$pid" || true)
  [ -n "${ticks:-}" ] || continue
  delta=$((ticks - start[$pid]))
  [ "$delta" -ge 0 ] || continue
  # Strictly greater than 10% of one core. Exactly 10% is still free.
  if [ $((delta * 10)) -gt "$limit" ]; then
    tenths=$((delta * 1000 / limit))
    printf 'BOX_BUSY_LOCALAI llama-server pid %s used %s.%s%% of one core over 5s\n' \
      "$pid" $((tenths / 10)) $((tenths % 10))
    exit 3
  fi
done

exit 0
