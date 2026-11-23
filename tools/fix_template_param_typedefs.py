#!/usr/bin/env python3
"""Fix `typedef P P;` inside class templates whose template parameter is also named P.

ISO C++ forbids redeclaring a template parameter; MSVC accepts it and uses it to expose the
parameter as a member type (Class::P). The parameter is renamed to P_ in the class template's
own parameter list and the typedef becomes `typedef P_ P;`, so Class::P keeps working.
Out-of-class member definitions declare their own parameters and need no change.
Usage: fix_template_param_typedefs.py <monolith/src>   (idempotent)"""
import os
import re
import sys

src = sys.argv[1]
changed = 0
for d, dirs, fs in os.walk(src):
    dirs[:] = [x for x in dirs if x != "3rd party"]
    for f in fs:
        if not f.endswith((".h", ".hpp", ".inl")):
            continue
        p = os.path.join(d, f)
        t = open(p, encoding="latin-1", newline="").read()
        orig = t
        while True:
            m = re.search(r"typedef\s+(\w+)\s+\1\s*;", t)
            if not m:
                break
            name = m.group(1)
            # walk back over `template <...>` headers (angle brackets balanced) to the nearest one
            # that declares `name` as a parameter; that is the template whose parameter is shadowed
            hit = None
            for h in re.finditer(r"template\s*<", t[: m.start()]):
                i, depth = h.end(), 1
                while depth and i < len(t):
                    depth += {"<": 1, ">": -1}.get(t[i], 0)
                    i += 1
                params = t[h.end(): i - 1]
                if re.search(r"\b(typename|class)\s+%s\b" % name, params):
                    hit = (h.end(), i - 1)
            if not hit:
                print("no template header for", p, name)
                break
            params = t[hit[0]: hit[1]]
            t = t[: hit[0]] + re.sub(r"\b%s\b" % name, name + "_", params) + t[hit[1]:]
            t = re.sub(r"typedef\s+%s\s+%s\s*;" % (name, name),
                       "typedef %s_ %s; // parameter renamed: ISO C++ forbids redeclaring it" % (name, name), t, count=1)
        if t != orig:
            open(p, "w", encoding="latin-1", newline="").write(t)
            changed += 1
print("files changed:", changed)
