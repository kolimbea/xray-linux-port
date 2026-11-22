#!/bin/bash
# Syntax-check every TU of a Monolith module with clang; print grouped first-cause errors.
# Usage: tools/probe.sh <module dir under monolith/src> [extra clang flags...]
set -u
mod=$1; shift
cd "$(dirname "$0")/../monolith/src/$mod" || exit 1
out=$(mktemp)
export CLANG_FLAGS="$*"
grep -o 'ClCompile Include="[^"]*"' *.vcxproj | sed 's/.*="//;s/"//;s#\\#/#g' | grep -vE '^blackbox/|borland' |
  xargs -P"$(nproc)" -I{} sh -c 'clang++ -std=c++20 -fsyntax-only -fms-extensions -fno-operator-names -Wno-register -w -ferror-limit=0 -DNDEBUG \
    -I../Common/linux/include -I. -I.. -I../../sdk/include -I"../3rd party" $CLANG_FLAGS "{}" 2>&1 | grep "error:" | sed "s#^#{}: #"' > "$out"
echo "errors: $(wc -l < "$out")  files: $(cut -d: -f1 "$out" | sort -u | wc -l)"
sed -E 's/^[^ ]+ //; s/^[^ ]*error: //' "$out" | sort | uniq -c | sort -rn | head -${TOP:-30}
cp "$out" /tmp/claude-1000/probe-$(echo "$mod" | tr / _).txt; rm "$out"
