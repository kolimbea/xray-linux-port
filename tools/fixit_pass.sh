#!/bin/bash
# Run clang over a module's TUs and apply allow-listed fix-its (tools/apply_fixits.py).
# Usage: tools/fixit_pass.sh <module dir under monolith/src> [extra clang flags]
set -u
mod=$1; shift
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root/monolith/src/$mod" || exit 1
proj=$(ls *.vcxproj | head -1)
defs=$(python3 ../../cmake/vcxproj_sources.py --defines "$proj" | sed 's/^/-D/' | tr '\n' ' ')
python3 ../../cmake/vcxproj_sources.py "$proj" | xargs -P"$(nproc)" -I{} sh -c \
  "clang++ -std=c++20 -fsyntax-only -fms-extensions -fno-operator-names -Wno-register -w -fdelayed-template-parsing \
   -ferror-limit=0 -fdiagnostics-parseable-fixits -DNDEBUG $defs $* \
   -I../Common/linux/include -I. -I.. -I../xrCore -I../xrEngine -I../../sdk/include -I'../3rd party' -I'../3rd party/ode/include' \
   '{}' 2>&1" | python3 "$root/tools/apply_fixits.py"
