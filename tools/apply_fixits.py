#!/usr/bin/env python3
"""Apply clang fix-its for an allow-listed set of diagnostics only (never "did you mean" guesses).

Reads clang output produced with -fdiagnostics-parseable-fixits on stdin, e.g.
    clang++ ... -fsyntax-only -fdiagnostics-parseable-fixits file.cpp 2>&1 | apply_fixits.py
Only fix-its that follow a diagnostic matching ALLOWED are applied. Edits are de-duplicated (headers
are seen from many TUs) and applied right-to-left per line. Prints a per-file count.
"""
import collections
import re
import sys

ALLOWED = [
    re.compile(r"missing 'typename' prior to dependent type name"),
    re.compile(r"template specialization requires 'template<>'"),
]
DIAG = re.compile(r"^(?P<file>[^:]+):(?P<line>\d+):(?P<col>\d+): (error|warning): (?P<msg>.*)$")
# Diagnostics clang reports without a fix-it but whose fix is a plain insertion at the reported spot.
INSERT_AT = [
    (re.compile(r"missing 'typename' prior to dependent type name"), "typename "),
    (re.compile(r"explicit qualification required to use member '.*' from dependent base class"), "this->"),
]
# "use of undeclared identifier 'x'" is only fixed when clang's next note confirms the name lives in
# a dependent base class (MSVC finds it there without this->).
UNDECLARED = re.compile(r"use of undeclared identifier '(\w+)'")
DEP_NOTE = re.compile(r"note: must qualify identifier to find this declaration in dependent base class")
FIX = re.compile(r'^fix-it:"(?P<file>[^"]+)":\{(?P<l1>\d+):(?P<c1>\d+)-(?P<l2>\d+):(?P<c2>\d+)\}:"(?P<text>.*)"$')

edits = collections.defaultdict(set)
allowed = False
pending = None  # (file, line, col) of an undeclared-identifier error awaiting its note
for line in sys.stdin:
    line = line.rstrip("\n")
    if pending and DEP_NOTE.search(line):
        f_, l_, c_ = pending
        edits[f_].add((l_, c_, l_, c_, "this->"))
        pending = None
        continue
    m = DIAG.match(line)
    if m:
        pending = None
        if UNDECLARED.search(m.group("msg")):
            pending = (m.group("file"), int(m.group("line")), int(m.group("col")))
        allowed = any(p.search(m.group("msg")) for p in ALLOWED)
        for pat, text in INSERT_AT:
            if pat.search(m.group("msg")):
                l, c = int(m.group("line")), int(m.group("col"))
                edits[m.group("file")].add((l, c, l, c, text))
        continue
    f = FIX.match(line)
    if f and allowed:
        text = bytes(f.group("text"), "utf-8").decode("unicode_escape")
        edits[f.group("file")].add((int(f.group("l1")), int(f.group("c1")), int(f.group("l2")), int(f.group("c2")), text))

for path, es in sorted(edits.items()):
    with open(path, encoding="latin-1", newline="") as fh:
        lines = fh.read().split("\n")
    applied = 0
    for l1, c1, l2, c2, text in sorted(es, reverse=True):
        if l1 != l2:
            continue  # multi-line replacements are not expected for these diagnostics
        s = lines[l1 - 1]
        if c1 == c2 and (s[c1 - 1:c1 - 1 + len(text)] == text or s[max(0, c1 - 1 - len(text)):c1 - 1] == text):
            continue  # insertion already present (re-run)
        lines[l1 - 1] = s[:c1 - 1] + text + s[c2 - 1:]
        applied += 1
    with open(path, "w", encoding="latin-1", newline="") as fh:
        fh.write("\n".join(lines))
    print(f"{applied:5d} {path}")
