#!/bin/bash
# Re-run the exact compile commands (build/monolith/compile_commands.json) of TUs whose path contains
# $1, syntax-only with parseable fix-its, and apply the allow-listed fixes (tools/apply_fixits.py).
set -u
root=$(cd "$(dirname "$0")/.." && pwd)
python3 - "$root/build/monolith/compile_commands.json" "$1" <<'PY' | python3 "$root/tools/apply_fixits.py"
import concurrent.futures, json, os, shlex, subprocess, sys
cmds = []
for c in json.load(open(sys.argv[1])):
    if sys.argv[2] not in c["file"]:
        continue
    a = shlex.split(c["command"])
    i = a.index("-o"); del a[i:i + 2]
    a.remove("-c")
    cmds.append((c["directory"], a + ["-fsyntax-only", "-ferror-limit=0", "-fdiagnostics-parseable-fixits", "-fno-color-diagnostics"]))
def run(dc):
    d, a = dc
    return subprocess.run(a, cwd=d, capture_output=True, text=True, errors="replace").stderr
with concurrent.futures.ThreadPoolExecutor(os.cpu_count()) as ex:
    for out in ex.map(run, cmds):
        sys.stdout.write(out)
PY
