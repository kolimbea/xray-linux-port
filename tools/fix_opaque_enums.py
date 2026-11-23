#!/usr/bin/env python3
"""Give opaque enum declarations (`enum X;`) and their definitions an explicit `: int` base.

ISO C++ allows forward-declaring an unscoped enum only with a fixed underlying type; MSVC accepts it
without one and always uses int, so adding `: int` everywhere keeps the Windows ABI and semantics.
Usage: fix_opaque_enums.py <monolith/src>   (skips "3rd party"; idempotent)"""
import os
import re
import sys

src = sys.argv[1]
EXTS = (".h", ".hpp", ".cpp", ".inl")
FWD = re.compile(r"\benum\s+([A-Za-z_]\w*)\s*;")
files = []
for d, dirs, fs in os.walk(src):
    dirs[:] = [x for x in dirs if x != "3rd party"]
    files += [os.path.join(d, f) for f in fs if f.endswith(EXTS)]

texts = {p: open(p, encoding="latin-1", newline="").read() for p in files}
names = set()
for t in texts.values():
    names.update(FWD.findall(t))

changed = 0
for p, t in texts.items():
    new = t
    for n in names:
        new = re.sub(r"\benum\s+%s\s*;" % n, "enum %s : int;" % n, new)
        # definitions: `enum Name {` or `enum Name\n{` (not already typed, not `enum class`)
        new = re.sub(r"\benum(\s+)%s(\s*)(?=\{)" % n, lambda m: "enum%s%s : int%s" % (m.group(1), n, m.group(2) or " "), new)
    if new != t:
        open(p, "w", encoding="latin-1", newline="").write(new)
        changed += 1
print(f"enums: {len(names)}  files changed: {changed}")
