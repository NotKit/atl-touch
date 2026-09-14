#!/usr/bin/env python3
"""Check that every camera2 method an app references resolves against ATL.

The reference list is one "package.Class#method" per line, as produced by
tools/dex-fw-refs.py for the app under test. A method counts as
resolvable when javap finds it on the class or on one of its superclasses —
that is all the runtime does to link a call site, so a method that is present
but throws still resolves.

usage: check-camera2-methods.py <methods.txt> [classpath.jar]
"""

import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
DEFAULT_JAR = REPO / "builddir/hax-stripped.jar"

_javap_cache = {}


def javap(classpath, name):
    """(declaration text, superclass) for a class, or (None, None) if absent."""
    if name in _javap_cache:
        return _javap_cache[name]

    result = subprocess.run(["javap", "-p", "-classpath", str(classpath), name],
                            capture_output=True, text=True)
    if result.returncode != 0:
        _javap_cache[name] = (None, None)
        return _javap_cache[name]

    match = re.search(r"\bextends\s+([\w.$]+)", result.stdout)
    parent = match.group(1) if match else None
    if parent in ("java.lang.Object", name):
        parent = None
    _javap_cache[name] = (result.stdout, parent)
    return _javap_cache[name]


def declares(text, cls, method):
    if method == "<init>":
        return re.search(re.escape(cls) + r"\(", text) is not None
    return re.search(r"[\s.]" + re.escape(method) + r"\(", text) is not None


def resolves(classpath, cls, method):
    name = cls
    while name:
        text, parent = javap(classpath, name)
        if text is None:
            return False
        if declares(text, name, method):
            return True
        name = parent
    return False


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__.strip().splitlines()[-1])
    methods = Path(sys.argv[1])
    classpath = Path(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_JAR

    if not classpath.exists():
        sys.exit(f"no {classpath} (run ninja -C builddir)")

    missing_classes = set()
    missing = []
    checked = 0

    for line in methods.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        cls, _, method = line.partition("#")
        checked += 1
        if javap(classpath, cls)[0] is None:
            missing_classes.add(cls)
            missing.append(line)
        elif not resolves(classpath, cls, method):
            missing.append(line)

    for entry in missing:
        print(f"missing: {entry}")
    for name in sorted(missing_classes):
        print(f"missing class: {name}")
    print(f"{checked - len(missing)}/{checked} camera2 method references resolve")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
