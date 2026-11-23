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
INSERT_AT = [(re.compile(r"missing 'typename' prior to dependent type name"), "typename ")]
FIX = re.compile(r'^fix-it:"(?P<file>[^"]+)":\{(?P<l1>\d+):(?P<c1>\d+)-(?P<l2>\d+):(?P<c2>\d+)\}:"(?P<text>.*)"$')

edits = collections.defaultdict(set)
allowed = False
for line in sys.stdin:
    line = line.rstrip("\n")
    m = DIAG.match(line)
    if m:
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
        if c1 == c2 and s[c1 - 1:c1 - 1 + len(text)] == text:
            continue  # insertion already present (re-run)
        lines[l1 - 1] = s[:c1 - 1] + text + s[c2 - 1:]
        applied += 1
    with open(path, "w", encoding="latin-1", newline="") as fh:
        fh.write("\n".join(lines))
    print(f"{applied:5d} {path}")
