#!/bin/bash
# Syntax-check TUs from compile_commands.json whose path contains $1 (optionally a random sample of
# $2 of them) and print grouped errors. Fast iteration without a full build.
set -u
root=$(cd "$(dirname "$0")/.." && pwd)
out=$(mktemp)
python3 - "$root/build/monolith/compile_commands.json" "$1" "${2:-0}" <<'PY' | xargs -0 -P"$(nproc)" -I{} sh -c '{} 2>&1' | grep -E "error:" > "$out"
import json, random, shlex, sys
cs = [c for c in json.load(open(sys.argv[1])) if sys.argv[2] in c["file"]]
n = int(sys.argv[3])
if n and n < len(cs):
    random.seed(1)
    cs = random.sample(cs, n)
for c in cs:
    a = shlex.split(c["command"])
    i = a.index("-o"); del a[i:i + 2]
    a.remove("-c")
    a += ["-fsyntax-only", "-fno-color-diagnostics"]
    sys.stdout.write("cd " + shlex.quote(c["directory"]) + " && " + " ".join(shlex.quote(x) for x in a) + "\0")
PY
echo "error lines: $(wc -l < "$out")"
sed -E 's#^.*/src/##; s#.*/include/c\+\+/16/#STD/#; s/:([0-9]+):[0-9]+:/:\1:/' "$out" | sort | uniq -c | sort -rn | cut -c1-170 | head -${TOP:-40}
rm "$out"
