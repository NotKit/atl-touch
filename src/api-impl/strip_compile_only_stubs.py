#!/usr/bin/env python3
"""Copy a jar, dropping compile-time-only stub classes.

Some api-impl classes exist only so framework sources compile against
SDK31+ types (e.g. View.setOnReceiveContentListener's parameter). Modern
apps that actually use those APIs bundle their own copies of the types
via androidx. If api-impl.jar also defines them, ART's duplicate-class
check (oat_file_manager.cc) rejects the app's compiled oat file and the
whole app falls back to the interpreter:

  W oat_file_manager.cc:510] Found duplicate classes, falling back to
    extracting from APK : <app>.apk
  W oat_file_manager.cc:512] NOTE: This wastes RAM and hurts startup
    performance.

So those stubs must never ship in the dex.
"""
import sys
import zipfile

def main():
    src, dst, *excluded = sys.argv[1:]
    with zipfile.ZipFile(src) as zin, \
         zipfile.ZipFile(dst, "w", zipfile.ZIP_DEFLATED) as zout:
        for info in zin.infolist():
            if info.filename in excluded:
                continue
            zout.writestr(info, zin.read(info.filename))

if __name__ == "__main__":
    main()
