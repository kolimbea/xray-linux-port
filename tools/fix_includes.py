#!/usr/bin/env python3
"""Rewrite #include paths in the Monolith tree so they work on case-sensitive filesystems.

- backslashes -> forward slashes (all includes);
- quoted includes whose target exists only with different letter case are rewritten to the
  real spelling, resolved against the including file's directory and then the module search roots.

Idempotent; byte-preserving apart from the include paths. Usage: fix_includes.py <monolith/src> [--dry-run]
"""
import os
import re
import sys

INC = re.compile(r'^(\s*#\s*include\s*)([<"])([^>"]+)([>"])', re.M)
EXTS = (".h", ".hpp", ".cpp", ".inl", ".c", ".cc", ".hxx", ".ch")


class CaseIndex:
    """dir -> {lowercase name: real name}, built lazily."""

    def __init__(self):
        self.cache = {}

    def entries(self, d):
        if d not in self.cache:
            try:
                self.cache[d] = {n.lower(): n for n in os.listdir(d)}
            except OSError:
                self.cache[d] = {}
        return self.cache[d]

    def resolve(self, base, rel):
        """Real-case spelling of rel under base, or None if no case-insensitive match."""
        cur, out = base, []
        for part in rel.split("/"):
            if part in ("", "."):
                continue
            if part == "..":
                cur = os.path.dirname(cur)
                out.append(part)
                continue
            real = self.entries(cur).get(part.lower())
            if real is None:
                return None
            out.append(real)
            cur = os.path.join(cur, real)
        return "/".join(out)


_module_files = {}


def unique_in_module(src, module, name):
    """Path of the only file called `name` (case-insensitive) inside src/module, else None."""
    if module in (".", "3rd party"):
        return None
    if module not in _module_files:
        idx = {}
        for d, _, fs in os.walk(os.path.join(src, module)):
            for f in fs:
                idx.setdefault(f.lower(), []).append(os.path.join(d, f))
        _module_files[module] = idx
    hits = _module_files[module].get(name.lower(), [])
    return hits[0] if len(hits) == 1 else None


def main():
    src = os.path.abspath(sys.argv[1])
    dry = "--dry-run" in sys.argv
    roots = [src, os.path.join(src, "..", "sdk", "include"), os.path.join(src, "3rd party")]
    # module directories are include roots too (vcxproj AdditionalIncludeDirectories use them)
    roots += [os.path.join(src, m) for m in sorted(os.listdir(src)) if os.path.isdir(os.path.join(src, m)) and m != "3rd party"]
    idx = CaseIndex()
    stats = {"slash": 0, "case": 0, "files": 0}
    for dirpath, _, files in os.walk(src):
        for fn in files:
            if not fn.lower().endswith(EXTS):
                continue
            path = os.path.join(dirpath, fn)
            # newline="" keeps CRLF untouched; latin-1 round-trips arbitrary bytes.
            with open(path, encoding="latin-1", newline="") as f:
                text = f.read()

            def fix(m):
                lead, o, inc, c = m.groups()
                new = inc.replace("\\", "/").strip()  # MSVC ignores surrounding blanks in the name
                if new != inc:
                    stats["slash"] += 1
                if o == '"':
                    # the including file's own module is searched first, as its vcxproj does
                    rel = os.path.relpath(dirpath, src).split(os.sep)[0]
                    own = [os.path.join(src, rel)] if rel not in (".", "3rd party") else []
                    bases = [dirpath] + own + roots
                    # an exact match anywhere means the include already works: keep it
                    if not any(os.path.exists(os.path.join(b, new)) for b in bases):
                        for base in bases:
                            real = idx.resolve(base, new)
                            if real:
                                stats["case"] += 1
                                new = real
                                break
                        else:
                            # MSVC also resolves "../../x.h" against directories of files higher in the
                            # include stack; fall back to the file's unique occurrence in its own module.
                            hit = unique_in_module(src, rel, os.path.basename(new))
                            if hit:
                                stats["moved"] = stats.get("moved", 0) + 1
                                new = os.path.relpath(hit, dirpath).replace(os.sep, "/")
                return f"{lead}{o}{new}{c}"

            out = INC.sub(fix, text)
            if out != text:
                stats["files"] += 1
                if not dry:
                    with open(path, "w", encoding="latin-1", newline="") as f:
                        f.write(out)
    print(stats)


if __name__ == "__main__":
    main()
