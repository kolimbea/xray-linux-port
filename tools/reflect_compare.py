#!/usr/bin/env python3
"""Compares two reflection dumps of tests/reflect_dump (reference = R4/vkd3d DXBC, candidate = Vulkan/DXC
SPIR-V) on the shaders both compiled. Per variable the engine reads, type data (size, class, type, rows,
columns, elements) must match; offsets must match in named cbuffers. $Globals offsets may differ: each
compiler lays out its own $Globals and the engine takes offsets from the same shader's reflection.
Resources must match by name and type (slots are informational: each compiler numbers its own).
Usage: reflect_compare.py <reference-dump> <candidate-dump>
"""
import sys
from collections import defaultdict


def load(path):
    shaders, cur = {}, None
    for line in open(path):
        w = line.split()
        if not w:
            continue
        if w[0] == 'FILE':
            cur = None
            if w[-1] == 'ok':
                cur = shaders[' '.join(w[1:-1])] = {'var': {}, 'res': {}, 'in': set()}
        elif cur is None:
            continue
        elif w[0] == 'VAR':
            # VAR cb name offset O size S class C type T rows R cols C elems E
            cur['var'][(w[1], w[2])] = dict(offset=int(w[4]), size=int(w[6]), cls=w[8], type=w[10], rows=w[12], cols=w[14],
                                            elems=w[16])
        elif w[0] == 'RES':
            cur['res'][w[1]] = dict(type=w[3], slot=int(w[5]), count=w[7], dim=w[9])
        elif w[0] == 'IN':
            cur['in'].add(w[1])
    return shaders


def main(ref_path, cand_path):
    ref, cand = load(ref_path), load(cand_path)
    common = sorted(set(ref) & set(cand))
    problems = defaultdict(list)
    slot_diffs = 0
    for f in common:
        r, c = ref[f], cand[f]
        for key, rv in r['var'].items():
            cb, name = key
            cv = c['var'].get(key)
            if cv is None:
                problems['variable missing in candidate'].append(f'{f}: {cb}.{name}')
                continue
            for k in ('size', 'cls', 'type', 'rows', 'cols', 'elems'):
                if rv[k] != cv[k]:
                    problems[f'variable {k} differs'].append(f'{f}: {cb}.{name} {rv[k]} vs {cv[k]}')
            if cb != '$Globals' and rv['offset'] != cv['offset']:
                problems['offset differs in a named cbuffer'].append(f'{f}: {cb}.{name} {rv["offset"]} vs {cv["offset"]}')
        for name, rr in r['res'].items():
            if rr['type'] == '0':  # cbuffer entries: covered by the variables
                continue
            cr = c['res'].get(name)
            if cr is None:
                problems['resource missing in candidate'].append(f'{f}: {name}')
                continue
            for k in ('type', 'count', 'dim'):
                if rr[k] != cr[k]:
                    problems[f'resource {k} differs'].append(f'{f}: {name} {rr[k]} vs {cr[k]}')
            slot_diffs += rr['slot'] != cr['slot']
        for name, cr in c['res'].items():
            if cr['type'] != '0' and name not in r['res']:
                problems['resource only in candidate'].append(f'{f}: {name}')
        if r['in'] != c['in']:
            problems['VS input semantics differ'].append(f'{f}: {sorted(r["in"])} vs {sorted(c["in"])}')
    print(f'reference ok {len(ref)}, candidate ok {len(cand)}, compared {len(common)}; resource slot differences: {slot_diffs}')
    for k, v in sorted(problems.items()):
        print(f'{k}: {len(v)}')
        for x in v[:8]:
            print('   ', x)
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1], sys.argv[2]))
