#!/usr/bin/env python3
"""Fix `typedef P P;` where P is a template parameter of the enclosing class template.

ISO C++ forbids redeclaring a template parameter; MSVC accepts it and uses it to expose the
parameter as a member type (Class::P). Within the class template's scope — its parameter list,
base clause and body — P is renamed to P_, and the typedef becomes `typedef P_ P;`, so code
outside (Class::P) and out-of-class member definitions (own parameter lists) are unaffected.
Usage: fix_template_param_typedefs.py <monolith/src>   (idempotent)"""
import os
import re
import sys

MARK = "// parameter renamed: ISO C++ forbids redeclaring it"


def match(t, i, open_c, close_c):
    depth = 1
    while i < len(t) and depth:
        depth += {open_c: 1, close_c: -1}.get(t[i], 0)
        i += 1
    return i  # index after the closing char


def fix(t, path):
    while True:
        m = re.search(r"typedef\s+(\w+)\s+\1\s*;", t)
        if not m:
            return t
        name = m.group(1)
        scope = None
        for h in re.finditer(r"template\s*<", t[: m.start()]):
            pend = match(t, h.end(), "<", ">")
            if not re.search(r"\b(typename|class)\s+%s\b" % name, t[h.end(): pend - 1]):
                continue
            brace = t.find("{", pend)
            if brace < 0 or brace > m.start():
                continue
            bend = match(t, brace + 1, "{", "}")
            if bend > m.end():
                scope = (h.start(), bend)  # innermost enclosing template declaring `name`
        if not scope:
            print("skip (no enclosing template):", path, name)
            return t
        a, b = scope
        body = re.sub(r"\b%s\b" % name, name + "_", t[a:b])
        body = re.sub(r"typedef\s+%s_\s+%s_\s*;" % (name, name), "typedef %s_ %s; %s" % (name, name, MARK), body, count=1)
        t = t[:a] + body + t[b:]


src = sys.argv[1]
changed = 0
for d, dirs, fs in os.walk(src):
    dirs[:] = [x for x in dirs if x != "3rd party"]
    for f in fs:
        if f.endswith((".h", ".hpp", ".inl")):
            p = os.path.join(d, f)
            t = open(p, encoding="latin-1", newline="").read()
            n = fix(t, p)
            if n != t:
                open(p, "w", encoding="latin-1", newline="").write(n)
                changed += 1
print("files changed:", changed)
