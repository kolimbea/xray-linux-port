#!/bin/bash
# Syntax-check every TU of a Monolith module with clang; print grouped first-cause errors.
# Usage: tools/probe.sh <module dir under monolith/src> [extra clang flags...]
set -u
mod=$1; shift
cd "$(dirname "$0")/../monolith/src/$mod" || exit 1
out=$(mktemp)
export CLANG_FLAGS="$* -fdelayed-template-parsing $(python3 ../../cmake/vcxproj_sources.py --defines "$(ls *.vcxproj | head -1)" | sed "s/^/-D/" | tr "\n" " ")"
python3 - "$(ls *.vcxproj | head -1)" <<'PY' |
import re, sys
# Sources built for Release|x64: skip items excluded there, and Windows/Borland-only files.
x = open(sys.argv[1], encoding="utf-8", errors="replace").read()
for m in re.finditer(r'<ClCompile Include="([^"]+)"\s*(/>|>(.*?)</ClCompile>)', x, re.S):
    body = m.group(3) or ""
    if re.search(r"ExcludedFromBuild Condition=\"'\$\(Configuration\)\|\$\(Platform\)'=='Release\|x64'\">true", body):
        continue
    f = m.group(1).replace("\\", "/")
    if f.startswith("blackbox/") or "borland" in f.lower():
        continue
    print(f)
PY
  xargs -P"$(nproc)" -I{} sh -c 'clang++ -std=c++20 -fsyntax-only -fms-extensions -fno-operator-names -Wno-register -w -ferror-limit=0 -DNDEBUG \
    -I../Common/linux/include -I. -I.. -I../xrCore -I../xrEngine -I../../sdk/include -I"../3rd party" -I"../3rd party/ode/include" $CLANG_FLAGS "{}" 2>&1 | grep "error:" | sed "s#^#{}: #"' > "$out"
echo "errors: $(wc -l < "$out")  files: $(cut -d: -f1 "$out" | sort -u | wc -l)"
sed -E 's/^[^ ]+ //; s/^[^ ]*error: //' "$out" | sort | uniq -c | sort -rn | head -${TOP:-30}
cp "$out" /tmp/claude-1000/probe-$(echo "$mod" | tr / _).txt; rm "$out"
