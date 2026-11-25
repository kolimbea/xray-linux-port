#!/usr/bin/env python3
"""Builds a run directory with GAMMA's data as Mod Organizer 2 presents it to the game, without MO2:
Anomaly's root (db/, gamedata/, appdata/) overlaid by every enabled mod of the GAMMA profile in priority
order, then MO2's overwrite/ folder. The install is only read.

- db/ and gamedata/: a tree of symlinks to the winning file of each path. Paths are lower-cased so the
  overlay merges like MO2 does on Windows (case-insensitive); the engine's Linux FS resolves names
  case-insensitively as well.
- appdata/: copies of the winning *.ltx/config files (the engine writes there, a symlink would write into
  the install); saves, logs and shader caches are not taken. user.ltx gets the test overrides below.
- fsgame.ltx: Anomaly's.

modlist.txt lists mods highest priority first; "+" = enabled; "*_separator" entries are not mods.
Usage: make_fixture_gamma.py [gamma-root] [out-dir]   (defaults: ~/Games/gamma, build/fixture-gamma)
"""
import os
import shutil
import sys

ROOT = os.path.expanduser(sys.argv[1] if len(sys.argv) > 1 else '~/Games/gamma')
OUT = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                                                          'build', 'fixture-gamma')
ANOMALY = os.path.join(ROOT, 'anomaly')
MO2 = os.path.join(ROOT, 'gamma')
PROFILE = os.path.join(MO2, 'profiles', 'G.A.M.M.A')
LINKED = ('db', 'gamedata')
APPDATA_SKIP = ('savedgames', 'logs', 'shaders_cache', 'screenshots', 'reshade-shaders')
# windowed, always rendering (the engine only renders while its window is active), a fixed size
# files the game writes inside gamedata (MO2 sends those writes to overwrite/): copies, not links
WRITABLE = ('gamedata/configs/axr_options.ltx',)
USER_OVERRIDES = {'g_always_active': 'on', 'rs_screenmode': 'windowed', 'vid_mode': '1280x720'}


def enabled_mods():
    mods = []
    with open(os.path.join(PROFILE, 'modlist.txt'), encoding='utf-8') as f:
        for line in f:
            line = line.rstrip('\n')
            if line.startswith('+') and not line.endswith('_separator'):
                mods.append(line[1:])
    mods.reverse()  # lowest priority first
    return mods


def layers():
    yield 'anomaly', ANOMALY
    for m in enabled_mods():
        path = os.path.join(MO2, 'mods', m)
        if os.path.isdir(path):
            yield m, path
        else:
            print(f'! enabled mod without a folder: {m}')
    yield 'overwrite', os.path.join(MO2, 'overwrite')


def main():
    winners = {}  # lower-cased relative path -> source file
    appdata = {}
    counts = []
    for name, base in layers():
        n = 0
        for top in os.listdir(base):
            low = top.lower()
            if low not in LINKED and low != 'appdata':
                continue
            for d, dirs, files in os.walk(os.path.join(base, top)):
                rel_dir = os.path.relpath(d, base).lower()
                if low == 'appdata':
                    parts = rel_dir.split(os.sep)
                    if len(parts) > 1 and parts[1] in APPDATA_SKIP:
                        dirs[:] = []
                        continue
                for f in files:
                    rel = os.path.join(rel_dir, f.lower())
                    (appdata if low == 'appdata' else winners)[rel] = os.path.join(d, f)
                    n += 1
        counts.append((name, n))

    if os.path.exists(OUT):
        for entry in ('db', 'gamedata'):
            shutil.rmtree(os.path.join(OUT, entry), ignore_errors=True)
    os.makedirs(OUT, exist_ok=True)
    for rel, src in winners.items():
        dst = os.path.join(OUT, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        if rel in WRITABLE:
            shutil.copyfile(src, dst)
        else:
            os.symlink(src, dst)
    for rel, src in appdata.items():
        dst = os.path.join(OUT, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copyfile(src, dst)
    os.makedirs(os.path.join(OUT, 'appdata', 'logs'), exist_ok=True)
    shutil.copyfile(os.path.join(ANOMALY, 'fsgame.ltx'), os.path.join(OUT, 'fsgame.ltx'))

    user = os.path.join(OUT, 'appdata', 'user.ltx')
    lines = open(user, encoding='latin-1').read().splitlines() if os.path.exists(user) else []
    kept = [l for l in lines if l.split(' ', 1)[0] not in USER_OVERRIDES]
    kept += [f'{k} {v}' for k, v in USER_OVERRIDES.items()]
    with open(user, 'w', encoding='latin-1') as f:
        f.write('\n'.join(kept) + '\n')

    layers_with_files = sum(1 for _, n in counts if n)
    print(f'{len(counts)} layers ({layers_with_files} with files), {len(winners)} linked files, '
          f'{len(appdata)} appdata files -> {OUT}')


if __name__ == '__main__':
    main()
