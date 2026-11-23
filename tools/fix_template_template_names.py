#!/usr/bin/env python3
"""Drop parameter names inside template template parameters: `template <typename _a> class X`
used as a parameter becomes `template <typename> class X`. Those names have no meaning, but when
they repeat an enclosing parameter name ISO C++ rejects them as shadowing (MSVC does not).
Only `template <...>` that is itself a template parameter (preceded by '<' or ',') is touched.
Usage: fix_template_template_names.py <monolith/src>   (idempotent)"""
import os
import re
import sys


def close(t, i):
    depth = 1
    while i < len(t) and depth:
        depth += {"<": 1, ">": -1}.get(t[i], 0)
        i += 1
    return i


def fix(t):
    out, pos = [], 0
    for m in re.finditer(r"template\s*<", t):
        if m.start() < pos:
            continue
        before = t[: m.start()].rstrip()
        # a line-continuation backslash may sit between parameters inside macros
        while before.endswith("\\"):
            before = before[:-1].rstrip()
        if not before or before[-1] not in "<,":
            continue
        end = close(t, m.end())
        inner = t[m.end(): end - 1]
        names = re.findall(r"\b(?:typename|class)\s+(\w+)", inner)
        # a name referenced again inside the list (e.g. by a default argument) must stay
        if any(len(re.findall(r"\b%s\b" % n, inner)) > 1 for n in names):
            continue
        new_inner = re.sub(r"\b(typename|class)\s+\w+", r"\1", inner)
        if new_inner != inner:
            out.append(t[pos: m.end()])
            out.append(new_inner)
            pos = end - 1
    out.append(t[pos:])
    return "".join(out)


src = sys.argv[1]
changed = 0
for d, dirs, fs in os.walk(src):
    dirs[:] = [x for x in dirs if x != "3rd party"]
    for f in fs:
        if f.endswith((".h", ".hpp", ".inl", ".cpp")):
            p = os.path.join(d, f)
            t = open(p, encoding="latin-1", newline="").read()
            n = fix(t)
            if n != t:
                open(p, "w", encoding="latin-1", newline="").write(n)
                changed += 1
print("files changed:", changed)
