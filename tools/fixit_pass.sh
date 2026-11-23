#!/bin/bash
# Re-run the exact compile commands (build/monolith/compile_commands.json) of TUs whose path matches
# a filter, syntax-only with parseable fix-its, and apply the allow-listed fixes (tools/apply_fixits.py).
# Usage: tools/fixit_pass.sh <path substring, e.g. /src/xrGame/>
set -u
root=$(cd "$(dirname "$0")/.." && pwd)
python3 - "$root/build/monolith/compile_commands.json" "$1" <<'PY' | xargs -0 -P"$(nproc)" -I{} sh -c '{} 2>&1' | python3 "$root/tools/apply_fixits.py"
import json, shlex, sys
for c in json.load(open(sys.argv[1])):
    if sys.argv[2] not in c["file"]:
        continue
    args = shlex.split(c["command"])
    # drop -o <obj> and -c, add syntax-only + parseable fix-its, run from the entry's directory
    out = []
    skip = False
    for a in args:
        if skip:
            skip = False
            continue
        if a == "-o":
            skip = True
            continue
        if a == "-c":
            continue
        out.append(a)
    out += ["-fsyntax-only", "-ferror-limit=0", "-fdiagnostics-parseable-fixits", "-fno-color-diagnostics"]
    sys.stdout.write("cd " + shlex.quote(c["directory"]) + " && " + " ".join(shlex.quote(a) for a in out) + "\0")
PY
