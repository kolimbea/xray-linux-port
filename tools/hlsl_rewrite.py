#!/usr/bin/env python3
"""Prototype of the source-level compat rules for DXC -> SPIR-V (to be ported into the engine's
shader pipeline). Rewrites a copy of a shader tree; the game's files are never touched.

Rule 1 (SPIR-V needs immediate texel offsets): `T.Load(loc, offset)` on a Texture2D/Texture2DArray
   with a non-literal offset becomes `T.Load((loc) + int3/int4((offset), 0[, 0]))`. Load's offset is
   added to the integer texel coordinate, so this is exact (D3D11 Load ignores the offset for
   out-of-range handling the same way: the sum is what gets bounds-checked).
Usage: hlsl_rewrite.py <src-root> <dst-root>
"""
import os, re, sys

DECL = re.compile(r'\b(Texture2D|Texture2DArray)\s*(?:<[^>]*>)?\s+(\w+)')

def split_args(s, start):
    """s[start] is just after '('; returns (args, index after ')') at depth 0."""
    depth, cur, args, i = 0, [], [], start
    while i < len(s):
        c = s[i]
        if c in '([{':
            depth += 1
        elif c in ')]}':
            if depth == 0:
                args.append(''.join(cur))
                return args, i + 1
            depth -= 1
        elif c == ',' and depth == 0:
            args.append(''.join(cur)); cur = []; i += 1; continue
        cur.append(c); i += 1
    return None, start

def literal(a):
    return re.fullmatch(r'\s*(int[234]\s*\(\s*-?\d+\s*(,\s*-?\d+\s*)*\)|-?\d+)\s*', a) is not None

def rewrite(text, kinds):
    out, pos = [], 0
    for m in re.finditer(r'\b(\w+)\s*\.\s*Load\s*\(', text):
        name = m.group(1)
        if name not in kinds or m.start() < pos:
            continue
        # macro bodies: the receiver is a macro parameter whose type is unknown here
        line_start = text.rfind('\n', 0, m.start()) + 1
        if text[line_start:m.start()].lstrip().startswith('#'):
            continue
        args, end = split_args(text, m.end())
        if not args or len(args) != 2 or literal(args[1]):
            continue
        pad = 'int3((%s), 0)' if kinds[name] == 'Texture2D' else 'int4((%s), 0, 0)'
        out.append(text[pos:m.start()])
        out.append('%s.Load((%s) + %s)' % (name, args[0].strip(), pad % args[1].strip()))
        pos = end
    out.append(text[pos:])
    return ''.join(out)

def main(src, dst):
    files = []
    for d, _, fs in os.walk(src):
        for f in fs:
            files.append(os.path.join(d, f))
    texts = {f: open(f, encoding='latin-1', newline='').read() for f in files}  # keep bytes and line ends
    kinds = {}
    for t in texts.values():  # declarations may live in a different include than the use
        for k, n in DECL.findall(t):
            kinds.setdefault(n, k)
    changed = 0
    for f, t in texts.items():
        n = rewrite(t, kinds)
        o = os.path.join(dst, os.path.relpath(f, src))
        os.makedirs(os.path.dirname(o), exist_ok=True)
        open(o, 'w', encoding='latin-1', newline='').write(n)
        changed += n != t
    print(f'{len(files)} files, {changed} rewritten')

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
