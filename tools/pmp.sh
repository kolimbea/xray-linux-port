#!/bin/bash
# Poor man's profiler: samples the stacks of a running process's main thread with eu-stack and prints
# the functions found most often (inclusive) and on top of the stack (exclusive).
# Usage: tools/pmp.sh <pid> [samples=200] [interval_s=0.15]   (eu-stack -1: the main thread)
set -u
PID=$1; N=${2:-200}; INT=${3:-0.15}
OUT=$(mktemp)
for i in $(seq "$N"); do
    eu-stack -1 -p "$PID" -n 40 2>/dev/null | grep -E "^#" | sed -E 's/^#[0-9]+ +0x[0-9a-f]+ +//; s/ - [0-9]+$//' > "$OUT.s"
    head -1 "$OUT.s" | sed 's/^/TOP /' >> "$OUT"
    sort -u "$OUT.s" | sed 's/^/ANY /' >> "$OUT"
    sleep "$INT"
done
echo "== on top of the stack (exclusive), $N samples"
grep '^TOP ' "$OUT" | cut -c5- | sed -E 's/\(.*//' | sort | uniq -c | sort -rn | head -25
echo "== anywhere in the stack (inclusive)"
grep '^ANY ' "$OUT" | cut -c5- | sed -E 's/\(.*//' | sort | uniq -c | sort -rn | head -45
rm -f "$OUT" "$OUT.s"
